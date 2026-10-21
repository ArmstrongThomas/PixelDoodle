<?php
namespace App\Services;
use App\Models\User;use Illuminate\Support\Facades\{DB,RateLimiter};use Illuminate\Support\Str;
final class Pairing {
 private const SCOPES=['gallery:read','favorites:write','artworks:submit'];
 private function now():int {return (int)floor(microtime(true)*1000);}
 private function token():string {return rtrim(strtr(base64_encode(random_bytes(32)),'+/','-_'),'=');}
 private function active($id):bool {return User::whereKey($id)->where('disabled',false)->whereNotNull('email_verified_at')->exists();}
 private function account($id):void {abort_unless($this->active($id),401);}
 private function rate($kind,$subject,$max,$seconds):void {$key='pair:'.$kind.':'.hash('sha256',(string)$subject);abort_if(RateLimiter::tooManyAttempts($key,$max),429);RateLimiter::hit($key,$seconds);}
 private function request(string $code){$code=strtoupper(str_replace(['-',' '],'',$code));abort_unless(preg_match('/^[A-HJ-NP-Z2-9]{8}$/',$code),400,'Invalid or expired code');$r=DB::table('pairing_requests')->where('user_hash',hash('sha256',$code))->where('state','pending')->where('expires','>',$this->now())->lockForUpdate()->first();abort_unless($r,400,'Invalid or expired code');return $r;}
 public function begin(string $label,array $scopes,string $subject):array {
  $this->rate('begin',$subject,5,60);abort_if(!trim($label)||strlen($label)>48||preg_match('/[\x00-\x1f\x7f]/',$label)||!count($scopes)||count($scopes)>3||array_diff($scopes,self::SCOPES),400);
  return DB::transaction(function()use($label,$scopes){$now=$this->now();DB::table('pairing_requests')->where('expires','<=',$now)->delete();abort_if(DB::table('pairing_requests')->count()>=1000,503);$device=$this->token();$code='';$alphabet='ABCDEFGHJKLMNPQRSTUVWXYZ23456789';for($i=0;$i<8;$i++)$code.=$alphabet[random_int(0,31)];
   DB::table('pairing_requests')->insert(['id'=>(string)Str::uuid(),'device_hash'=>hash('sha256',$device),'user_hash'=>hash('sha256',$code),'label'=>trim($label),'scopes'=>json_encode(array_values(array_unique($scopes))),'expires'=>$now+600000,'next_poll'=>$now,'interval_ms'=>5000,'state'=>'pending']);return ['deviceCode'=>$device,'userCode'=>substr($code,0,4).'-'.substr($code,4),'expiresIn'=>600,'interval'=>5,'verificationUri'=>rtrim(config('app.url'),'/').'/#account'];
  });
 }
 public function inspect($account,string $code):array {$this->account($account);$this->rate('approve',$account,20,3600);$r=$this->request($code);return ['label'=>$r->label,'scopes'=>json_decode($r->scopes,true),'expiresAt'=>$r->expires];}
 public function decide($account,string $code,bool $approve):array {$this->account($account);$this->rate('approve',$account,20,3600);return DB::transaction(function()use($account,$code,$approve){$r=$this->request($code);$state=$approve?'approved':'denied';DB::table('pairing_requests')->where('id',$r->id)->update(['state'=>$state,'account'=>$account]);return ['state'=>$state];});}
 public function poll(string $code,string $subject):array {
  $this->rate('poll',$subject,120,60);abort_unless(preg_match('/^[A-Za-z0-9_-]{43}$/',$code),400);
  return DB::transaction(function()use($code){$now=$this->now();$r=DB::table('pairing_requests')->where('device_hash',hash('sha256',$code))->lockForUpdate()->first();if(!$r||$r->expires<=$now||$r->state==='consumed')return ['error'=>'expired_token'];
   if($now<$r->next_poll){$interval=min(60000,$r->interval_ms+5000);DB::table('pairing_requests')->where('id',$r->id)->update(['next_poll'=>$now+$interval,'interval_ms'=>$interval]);return ['error'=>'slow_down','interval'=>$interval/1000];}
   DB::table('pairing_requests')->where('id',$r->id)->update(['next_poll'=>$now+$r->interval_ms]);if($r->state==='pending')return ['error'=>'authorization_pending'];if($r->state==='denied'||!$this->active($r->account))return ['error'=>'access_denied'];
   if(DB::table('device_grants')->where('account',$r->account)->where('revoked',false)->where('expires','>',$now)->count()>=20)return ['error'=>'device_limit'];
   $access=$this->token();$id=(string)Str::uuid();DB::table('device_grants')->insert(['id'=>$id,'account'=>$r->account,'label'=>$r->label,'token_hash'=>hash('sha256',$access),'scopes'=>$r->scopes,'created'=>$now,'expires'=>$now+30*86400000,'revoked'=>false]);DB::table('pairing_requests')->where('id',$r->id)->update(['state'=>'consumed']);return ['accessToken'=>$access,'tokenType'=>'Bearer','expiresIn'=>30*86400,'scopes'=>json_decode($r->scopes,true),'deviceId'=>$id];
  });
 }
 public function devices($account):array {$this->account($account);return DB::table('device_grants')->where('account',$account)->orderByDesc('created')->get(['id','label','scopes','created','expires','revoked'])->map(fn($r)=>array_merge((array)$r,['scopes'=>json_decode($r->scopes,true)]))->all();}
 public function revoke($account,string $id):bool {$this->account($account);return DB::table('device_grants')->where('id',$id)->where('account',$account)->update(['revoked'=>true])>0;}
 public function authenticateGrant(string $token):?array {if(!preg_match('/^[A-Za-z0-9_-]{43}$/',$token))return null;$r=DB::table('device_grants')->where('token_hash',hash('sha256',$token))->where('revoked',false)->where('expires','>',$this->now())->first();if(!$r||!$this->active($r->account))return null;return ['accountId'=>(string)$r->account,'deviceId'=>$r->id,'scopes'=>json_decode($r->scopes,true)];}
 public function authenticate(string $token,string $scope):?array {if(!in_array($scope,self::SCOPES,true))return null;$grant=$this->authenticateGrant($token);if(!$grant||!in_array($scope,$grant['scopes'],true))return null;unset($grant['scopes']);return $grant;}
}
