<?php
namespace Tests\Feature;
use Tests\TestCase;use Illuminate\Foundation\Testing\RefreshDatabase;use Illuminate\Support\Facades\{DB,Hash,Mail};use App\Models\User;use App\Services\Pairing;
final class AccountTest extends TestCase {
 use RefreshDatabase;
 protected function setUp():void {$_ENV['AUTH_ENABLED']='true';parent::setUp();config(['app.url'=>'https://pixels.7db.pw','app.key'=>'base64:'.base64_encode(str_repeat('t',32)),'pixels.email_enabled'=>true,'mail.default'=>'smtp']);$this->withServerVariables(['HTTP_HOST'=>'pixels.7db.pw','HTTPS'=>'on'])->withHeaders(['Origin'=>'https://pixels.7db.pw']);}
 protected function tearDown():void {unset($_ENV['AUTH_ENABLED']);parent::tearDown();}
 public function test_discord_authorization_uses_only_identity_scopes_and_fixed_callback():void {
  config(['services.discord.client_id'=>'synthetic','services.discord.client_secret'=>'synthetic','services.discord.redirect'=>'https://pixels.7db.pw/api/auth/callback/discord']);
  $r=$this->postJson('https://pixels.7db.pw/api/auth/sign-in/social',['provider'=>'discord','callbackURL'=>'https://evil.test']);$r->assertOk();parse_str(parse_url($r->json('url'),PHP_URL_QUERY),$q);$this->assertSame('identify email',$q['scope']);$this->assertSame('https://pixels.7db.pw/api/auth/callback/discord',$q['redirect_uri']);$this->assertNotEmpty($q['state']);
  $this->getJson('https://pixels.7db.pw/api/auth/callback/discord?state=invalid&code=synthetic')->assertStatus(400);
 }
 public function test_csrf_is_required_outside_unit_test_bypass():void {
  $this->app->instance('env','production');
  $this->postJson('https://pixels.7db.pw/api/auth/sign-out',[])->assertStatus(419);
  $this->withSession(['_token'=>'test-csrf'])->withHeader('X-CSRF-TOKEN','test-csrf')->postJson('https://pixels.7db.pw/api/auth/sign-out',[])->assertOk();
 }
 public function test_native_pairing_is_opt_in_and_has_bounded_input():void {
  $this->postJson('https://pixels.7db.pw/api/device/pair/begin',['label'=>'DS'])->assertStatus(503);config(['pixels.device_pairing'=>true]);
  $r=$this->postJson('https://pixels.7db.pw/api/device/pair/begin',['label'=>'DS']);$r->assertOk();$this->assertSame('https://pixels.7db.pw/#account',$r->json('verificationUri'));
  $this->postJson('https://pixels.7db.pw/api/device/pair/poll',['deviceCode'=>$r->json('deviceCode')])->assertJsonPath('error','authorization_pending');
  $this->postJson('https://pixels.7db.pw/api/device/pair/begin',['label'=>'DS','subject'=>'spoof'])->assertStatus(400);
  $this->postJson('https://pixels.7db.pw/api/device/pair/begin',['label'=>str_repeat('x',1100)])->assertStatus(413);
 }
 public function test_verified_email_login_replay_logout_and_session_cookie():void {
  Mail::fake();$this->getJson('https://pixels.7db.pw/api/account/status')->assertOk()->assertJsonPath('enabled',true);
  $this->postJson('https://pixels.7db.pw/api/auth/email-otp/send-verification-otp',['email'=>'artist@example.test','type'=>'sign-in'])->assertOk();
  $this->assertSame(1,DB::table('email_codes')->count());
  DB::table('email_codes')->where('email','artist@example.test')->update(['digest'=>Hash::make('123456')]);
  $this->postJson('https://pixels.7db.pw/api/auth/sign-in/email-otp',['email'=>'artist@example.test','otp'=>'123456'])->assertOk();$this->assertAuthenticated();
  $this->postJson('https://pixels.7db.pw/api/auth/sign-in/email-otp',['email'=>'artist@example.test','otp'=>'123456'])->assertStatus(400);
  $this->postJson('https://pixels.7db.pw/api/auth/sign-out',[])->assertOk();$this->assertGuest();
 }
 public function test_wrong_code_limit_and_origin():void {
  DB::table('email_codes')->insert(['email'=>'artist@example.test','digest'=>Hash::make('123456'),'attempts'=>0,'expires'=>time()+300]);
  for($i=0;$i<3;$i++)$this->postJson('https://pixels.7db.pw/api/auth/sign-in/email-otp',['email'=>'artist@example.test','otp'=>'000000'])->assertStatus(400);
  $this->postJson('https://pixels.7db.pw/api/auth/sign-in/email-otp',['email'=>'artist@example.test','otp'=>'123456'])->assertStatus(400);
  $this->withHeader('Origin','https://evil.test')->postJson('https://pixels.7db.pw/api/auth/sign-out',[])->assertForbidden();
 }
 public function test_device_owner_approval_and_revoke():void {
  $u=User::factory()->create();$p=app(Pairing::class);$code=$p->begin('DS',['gallery:read'],'test');
  $this->postJson('https://pixels.7db.pw/api/account/pair/decide',['code'=>$code['userCode'],'approve'=>true])->assertUnauthorized();
  $this->actingAs($u)->postJson('https://pixels.7db.pw/api/account/pair/decide',['code'=>$code['userCode'],'approve'=>true,'accountId'=>'attacker'])->assertOk();
  $grant=$p->poll($code['deviceCode'],'test');$this->assertSame((string)$u->id,$p->authenticate($grant['accessToken'],'gallery:read')['accountId']);$this->assertNull($p->authenticate($grant['accessToken'],'artworks:submit'));
  $this->getJson('https://pixels.7db.pw/api/account/devices')->assertJsonCount(1,'devices');$this->postJson('https://pixels.7db.pw/api/account/devices/revoke',['id'=>$grant['deviceId']])->assertJsonPath('revoked',true);$this->assertNull($p->authenticate($grant['accessToken'],'gallery:read'));
 }
 public function test_pending_backoff_denial_and_disabled_account():void {
  $u=User::factory()->create();$p=app(Pairing::class);$c=$p->begin('DS',['gallery:read'],'test');$this->assertSame('authorization_pending',$p->poll($c['deviceCode'],'test')['error']);$this->assertSame('slow_down',$p->poll($c['deviceCode'],'test')['error']);
  $p->decide($u->id,$c['userCode'],false);DB::table('pairing_requests')->update(['next_poll'=>0]);$this->assertSame('access_denied',$p->poll($c['deviceCode'],'test')['error']);
  $c=$p->begin('3DS',['gallery:read'],'test');$p->decide($u->id,$c['userCode'],true);$u->disabled=true;$u->save();$this->assertSame('access_denied',$p->poll($c['deviceCode'],'test')['error']);
 }
}

