<?php
namespace App\Http\Controllers;
use App\Models\User;use Illuminate\Http\Request;use Illuminate\Support\Facades\{Auth,DB,Hash,Mail,RateLimiter};use Illuminate\Support\Str;use Laravel\Socialite\Facades\Socialite;
final class AccountController {
 public function status(Request $r){$u=$r->user();if($u?->disabled){Auth::logout();$r->session()->invalidate();$u=null;}return ['enabled'=>true,'providers'=>array_values(array_filter([config('services.discord.client_id')&&config('services.discord.client_secret')?'discord':null,config('pixels.email_enabled')?'email':null])),'csrfToken'=>csrf_token(),'isModerator'=>$u?User::whereKey($u->id)->where('is_moderator',true)->where('disabled',false)->whereNotNull('email_verified_at')->exists():false,'user'=>$u?['id'=>(string)$u->id,'name'=>$u->name,'email'=>$u->email]:null];}
 private function limit(string $key,int $max,int $seconds):void {abort_if(RateLimiter::tooManyAttempts($key,$max),429);RateLimiter::hit($key,$seconds);}
 public function discord(Request $r){$r->validate(['provider'=>'required|in:discord']);abort_unless(config('services.discord.client_id')&&config('services.discord.client_secret'),503);$this->limit('oauth:'.$r->ip(),20,60);$target=Socialite::driver('discord')->setScopes(['identify','email'])->withConsent()->redirect();return ['url'=>$target->getTargetUrl()];}
 public function callback(Request $r){
  if($r->has('error')){$r->session()->forget('state');return redirect('/#account');}
  try{$identity=Socialite::driver('discord')->user();}catch(\Throwable){abort(400,'Discord sign-in could not be verified. Please try again.');}
  abort_unless(($identity->user['verified']??false)===true&&filter_var($identity->getEmail(),FILTER_VALIDATE_EMAIL)&&preg_match('/^\d{1,32}$/',(string)$identity->getId()),403,'A verified Discord email is required.');
  $email=strtolower($identity->getEmail());
  $user=DB::transaction(function()use($identity,$email){$u=User::where('discord_id',$identity->getId())->first();if($u)return $u;abort_if(User::where('email',$email)->exists(),409,'Sign in using the existing account method; account linking is not enabled yet.');$u=new User();$u->name=mb_substr($identity->getName()?:$identity->getNickname()?:'Artist',0,80);$u->email=$email;$u->email_verified_at=now();$u->password=Str::random(64);$u->discord_id=$identity->getId();$u->signin_method='discord';$u->save();return $u;});
  abort_if($user->disabled,403);Auth::login($user);$r->session()->regenerate();return redirect('/#account');
 }
 public function sendCode(Request $r){
  abort_unless(config('pixels.email_enabled')&&config('mail.default')==='smtp',503);
  $v=$r->validate(['email'=>'required|email|max:254','type'=>'required|in:sign-in']);$email=strtolower(trim($v['email']));
  $this->limit('email-ip:'.$r->ip(),10,600);$this->limit('email-address:'.hash('sha256',$email),3,600);
  $code=(string)random_int(100000,999999);$digest=Hash::make($code);
  DB::table('email_codes')->updateOrInsert(['email'=>$email],['digest'=>$digest,'attempts'=>0,'expires'=>time()+300]);
  try{Mail::raw("Your Pixel Doodle sign-in code is $code. It expires in five minutes. If you did not request it, ignore this email.",fn($m)=>$m->to($email)->subject('Pixel Doodle sign-in code'));}catch(\Throwable){DB::table('email_codes')->where('email',$email)->where('digest',$digest)->delete();abort(503,'Email could not be delivered.');}
  return ['sent'=>true];
 }
 public function verifyCode(Request $r){
  abort_unless(config('pixels.email_enabled'),503);$v=$r->validate(['email'=>'required|email|max:254','otp'=>'required|digits:6']);$email=strtolower(trim($v['email']));$this->limit('verify:'.$r->ip(),20,60);
  $valid=DB::transaction(function()use($email,$v){$c=DB::table('email_codes')->where('email',$email)->lockForUpdate()->first();if(!$c||$c->expires<=time()||$c->attempts>=3)return false;DB::table('email_codes')->where('email',$email)->increment('attempts');if(!Hash::check($v['otp'],$c->digest))return false;DB::table('email_codes')->where('email',$email)->delete();return true;});
  abort_unless($valid,400,'Invalid or expired code.');
  $u=User::firstOrNew(['email'=>$email]);abort_if($u->exists&&($u->signin_method!=='email'||$u->disabled),403,'Use the existing account sign-in method.');
  if(!$u->exists){$u->name='Artist';$u->password=Str::random(64);$u->email_verified_at=now();$u->save();}Auth::login($u);$r->session()->regenerate();return ['user'=>['id'=>(string)$u->id,'name'=>$u->name]];
 }
 public function logout(Request $r){Auth::logout();$r->session()->invalidate();$r->session()->regenerateToken();return ['success'=>true];}
}
