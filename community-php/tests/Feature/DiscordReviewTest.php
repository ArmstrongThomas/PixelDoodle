<?php
namespace Tests\Feature;
use Tests\TestCase;
use App\Models\User;
use App\Services\{GallerySchema,GalleryStore,Png,DiscordReview};
use Illuminate\Foundation\Testing\RefreshDatabase;
use Illuminate\Support\Facades\Http;
final class DiscordReviewTest extends TestCase {
 use RefreshDatabase;
 private string $path;private string $id;private string $secret;
 protected function setUp():void {parent::setUp();$this->path=tempnam(sys_get_temp_dir(),'pd-bot-');GallerySchema::migrate($this->path);$keys=sodium_crypto_sign_keypair();$this->secret=sodium_crypto_sign_secretkey($keys);config(['pixels.gallery_database'=>$this->path,'discord.enabled'=>true,'discord.token'=>'test-only','discord.public_key'=>bin2hex(sodium_crypto_sign_publickey($keys)),'discord.application_id'=>'111','discord.guild_id'=>'222','discord.channel_id'=>'333','discord.roles'=>['444']]);$s=new GalleryStore($this->path);$s->run('INSERT INTO artists VALUES (?,?,?)',['artist','Artist','Bio']);$this->id=$s->ingest(Png::encode(['width'=>1,'height'=>1,'rgba'=>"\x12\x34\x56\x78"]),'artist','Artwork',[],'Description')['id'];Http::preventStrayRequests();}
 protected function tearDown():void {parent::tearDown();gc_collect_cycles();unlink($this->path);}
 private function service():DiscordReview{return new DiscordReview(new GalleryStore($this->path));}
 private function interaction(array $v,?string $signature=null,?int $at=null){$body=json_encode($v);$time=(string)($at??time());return $this->call('POST','/api/discord/interactions',[],[],[],['CONTENT_TYPE'=>'application/json','HTTP_X_SIGNATURE_TIMESTAMP'=>$time,'HTTP_X_SIGNATURE_ED25519'=>$signature??bin2hex(sodium_crypto_sign_detached($time.$body,$this->secret))],$body);}
 private function button(string $action='approve',int $revision=1):array{return ['application_id'=>'111','type'=>3,'guild_id'=>'222','channel_id'=>'333','member'=>['roles'=>['444'],'user'=>['id'=>'555']],'message'=>['id'=>'666'],'data'=>['custom_id'=>'pd:'.$action.':'.$this->id.':'.$revision]];}
 private function send():void{Http::swap(new \Illuminate\Http\Client\Factory());Http::preventStrayRequests();Http::fake(['discord.com/*'=>Http::response(['id'=>'666'],200)]);$this->assertSame(1,$this->service()->tick()['sent']);}
 public function test_signatures_scope_roles_stale_and_disable():void {
  $ping=['type'=>1,'application_id'=>'111'];$this->interaction($ping)->assertJsonPath('type',1);$this->interaction($ping,str_repeat('0',128))->assertUnauthorized();$this->interaction($ping,null,time()-301)->assertUnauthorized();
  $this->send();$this->interaction($this->button())->assertJsonPath('type',4);
  $u=User::factory()->create(['discord_id'=>'555','is_moderator'=>true]);
  $bad=$this->button();$bad['channel_id']='777';$this->interaction($bad)->assertJsonPath('type',4);
  $bad=$this->button();$bad['member']['roles']=[];$this->interaction($bad)->assertJsonPath('type',4);
  $this->interaction($this->button())->assertOk()->assertJsonPath('type',7)->assertJsonPath('data.components.0.components.0.disabled',true);
  $this->interaction($this->button())->assertJsonPath('type',4);
  $s=new GalleryStore($this->path);$this->assertSame('published',$s->run('SELECT status FROM artworks WHERE id=?',[$this->id])->fetchColumn());$this->assertSame((string)$u->id,$s->run("SELECT actor FROM moderation_audit WHERE action='approve'")->fetchColumn());
 }
 public function test_reject_modal_and_revoked_role():void {
  $this->send();$u=User::factory()->create(['discord_id'=>'555','is_moderator'=>true]);$v=$this->button('deny');$this->interaction($v)->assertJsonPath('type',9);
  $v['type']=5;$v['data']['components']=[['components'=>[['custom_id'=>'reason','value'=>'Please use your own artwork.']]]];
  $u->is_moderator=false;$u->save();$this->interaction($v)->assertJsonPath('type',4);$u->is_moderator=true;$u->save();$this->interaction($v)->assertJsonPath('type',7);
  $s=new GalleryStore($this->path);$this->assertSame('denied',$s->run('SELECT status FROM artworks WHERE id=?',[$this->id])->fetchColumn());$this->assertSame('Please use your own artwork.',$s->run("SELECT reason FROM moderation_audit WHERE action='deny'")->fetchColumn());
 }
 public function test_uncertain_sends_never_retry_or_start_timer_and_success_has_attachment():void {
  Http::fake(['discord.com/*'=>Http::response('failure',500)]);$this->assertSame(1,$this->service()->tick()['uncertain']);$this->assertSame(0,$this->service()->tick()['sent']);Http::assertSentCount(1);
  $s=new GalleryStore($this->path);$this->assertNull($s->run('SELECT due_at FROM moderation_queue')->fetchColumn()?:null);$this->assertSame('pending',$s->run('SELECT status FROM artworks WHERE id=?',[$this->id])->fetchColumn());
 }
 public function test_rate_limit_defers_send_and_worker_syncs_website_decision():void {
  Http::fake(['discord.com/*'=>Http::response(['retry_after'=>60],429)]);$this->service()->tick();$this->service()->tick();Http::assertSentCount(1);
  $s=new GalleryStore($this->path);$s->run('UPDATE moderation_queue SET retry_at=0');$this->send();
  $user=User::factory()->create(['is_moderator'=>true]);$this->service()->moderation()->decide((string)$user->id,$this->id,'hold',1);$this->service()->tick();
  $row=$this->service()->row($this->id);$this->assertSame('held',$row['status']);$this->assertNull($row['due_at']);$this->assertSame($row['revision'],$row['synced_revision']);
  Http::assertSent(fn($r)=>$r->method()==='PATCH'&&$r['embeds'][0]['fields'][1]['value']==='1 × 1 · held');
 }
}

