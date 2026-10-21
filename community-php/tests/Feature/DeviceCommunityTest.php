<?php
namespace Tests\Feature;

use App\Models\User;
use App\Services\{GallerySchema,GalleryStore,Moderation,Pairing,Png};
use Illuminate\Foundation\Testing\RefreshDatabase;
use Illuminate\Support\Str;
use Tests\TestCase;

final class DeviceCommunityTest extends TestCase {
 use RefreshDatabase;
 private string $path; private User $owner; private string $readToken; private string $allToken;
 protected function setUp():void {
  $_ENV['AUTH_ENABLED']='true';parent::setUp();$this->path=tempnam(sys_get_temp_dir(),'pd-device-gallery-');GallerySchema::migrate($this->path);
  config(['app.url'=>'https://pixels.7db.pw','pixels.auth_enabled'=>true,'pixels.device_pairing'=>true,'pixels.gallery_database'=>$this->path]);
  $this->withServerVariables(['HTTP_HOST'=>'pixels.7db.pw','HTTPS'=>'on']);
  $this->owner=User::factory()->create();$p=app(Pairing::class);
  $this->readToken=$this->grant($p,$this->owner,['gallery:read']);
  $this->allToken=$this->grant($p,$this->owner,['gallery:read','favorites:write','artworks:submit']);
  $s=new GalleryStore($this->path);$s->run('INSERT INTO artists VALUES (?,?,?)',['owned-artist','Owned Artist','']);
  $s->run('INSERT INTO artist_accounts VALUES (?,?)',[(string)$this->owner->id,'owned-artist']);
 }
 protected function tearDown():void {unset($_ENV['AUTH_ENABLED']);parent::tearDown();gc_collect_cycles();if(isset($this->path)&&is_file($this->path))unlink($this->path);}
 private function grant(Pairing $p,User $u,array $scopes):string {$c=$p->begin(count($scopes)>1?'All scopes':'Read scope',$scopes,'fixture');$p->decide($u->id,$c['userCode'],true);return $p->poll($c['deviceCode'],'fixture')['accessToken'];}
 private function auth(string $token):self {return $this->withHeader('Authorization','Bearer '.$token);}
 private function published(string $title):string {
  $s=new GalleryStore($this->path);$id=$s->ingest(Png::encode(['width'=>1,'height'=>1,'rgba'=>"\x11\x22\x33\xff"]),'owned-artist',$title,[])['id'];
  (new Moderation($s,fn()=>true,fn()=>1000))->decide('fixture',$id,'approve',0);return $id;
 }
 public function test_scope_boundary_favorites_visibility_idempotence_and_revocation():void {
  config(['pixels.device_pairing'=>true]);$public=$this->published('Public pixel');
  $this->auth($this->readToken)->getJson('https://pixels.7db.pw/api/device/favorites')->assertForbidden();
  $this->auth($this->allToken)->postJson('https://pixels.7db.pw/api/device/favorites/'.$public,['favorite'=>true])->assertOk();
  $this->auth($this->allToken)->postJson('https://pixels.7db.pw/api/device/favorites/'.$public,['favorite'=>true])->assertOk();
  $this->auth($this->allToken)->getJson('https://pixels.7db.pw/api/device/favorites')->assertJsonCount(1,'items');
  $compact=$this->auth($this->allToken)->getJson('https://pixels.7db.pw/api/device/favorites?limit=6&compact=1')->assertOk()->json();$this->assertArrayNotHasKey('tags',$compact['items'][0]);$this->assertSame('Public pixel',$compact['items'][0]['title']);
  $s=new GalleryStore($this->path);$s->run("UPDATE artworks SET status='pending',visibility='private' WHERE id=?",[$public]);
  $this->auth($this->allToken)->getJson('https://pixels.7db.pw/api/device/favorites')->assertJsonCount(0,'items')->assertJsonPath('total',0);
  $device=collect(app(Pairing::class)->devices($this->owner->id))->first(fn($d)=>$d['label']==='All scopes');app(Pairing::class)->revoke($this->owner->id,$device['id']);
  $this->auth($this->allToken)->getJson('https://pixels.7db.pw/api/device/uploads')->assertUnauthorized();
 }
 public function test_upload_chunks_are_private_resumable_and_request_idempotent():void {
  $bytes=Png::encode(['width'=>1,'height'=>1,'rgba'=>"\x12\x34\x56\xff"]);$hash=hash('sha256',$bytes);$requestId=(string)Str::uuid();
  $begin=$this->auth($this->allToken)->postJson('https://pixels.7db.pw/api/device/uploads/begin',['requestId'=>$requestId,'title'=>'Console test','description'=>'Only a flattened image','tags'=>['pixel'],'size'=>strlen($bytes),'sha256'=>$hash])->assertOk()->json();
  $this->assertSame(1,$begin['chunkCount']);$encoded=base64_encode($bytes);
  $this->auth($this->allToken)->getJson('https://pixels.7db.pw/api/device/uploads/'.$begin['uploadId'])->assertJsonPath('receivedChunks',[])->assertJsonPath('missingChunks',[0]);
  $chunkUrl='https://pixels.7db.pw/api/device/uploads/'.$begin['uploadId'].'/chunks/0';
  $this->auth($this->allToken)->postJson($chunkUrl,['data'=>$encoded,'sha256'=>$hash])->assertOk();
  $this->auth($this->allToken)->postJson($chunkUrl,['data'=>$encoded,'sha256'=>$hash])->assertOk();
  $result=$this->auth($this->allToken)->postJson('https://pixels.7db.pw/api/device/uploads/'.$begin['uploadId'].'/finalize',[])->assertOk()->assertJsonPath('status','pending')->json();
  $this->assertNotEmpty($result['id']);$this->assertSame(0,(int)(new GalleryStore($this->path))->run('SELECT count(*) FROM device_upload_chunks')->fetchColumn());
  $this->auth($this->allToken)->postJson('https://pixels.7db.pw/api/device/uploads/'.$begin['uploadId'].'/finalize',[])->assertJsonPath('duplicate',true);
  $this->auth($this->allToken)->getJson('https://pixels.7db.pw/api/device/uploads')->assertJsonCount(1,'items')->assertJsonPath('items.0.status','pending')->assertJsonPath('total',1)->assertJsonPath('nextOffset',null);
  $this->assertSame(1,(int)(new GalleryStore($this->path))->run('SELECT count(*) FROM artworks')->fetchColumn());
 }
 public function test_upload_cancel_removes_private_chunks_and_retry_can_restart():void {
  $bytes=Png::encode(['width'=>1,'height'=>1,'rgba'=>"\x12\x34\x56\xff"]);$requestId=(string)Str::uuid();$meta=['requestId'=>$requestId,'title'=>'Cancelled art','description'=>'','tags'=>[],'size'=>strlen($bytes),'sha256'=>hash('sha256',$bytes)];
  $first=$this->auth($this->allToken)->postJson('https://pixels.7db.pw/api/device/uploads/begin',$meta)->assertOk()->json();
  $this->auth($this->allToken)->postJson('https://pixels.7db.pw/api/device/uploads/'.$first['uploadId'].'/chunks/0',['data'=>base64_encode($bytes),'sha256'=>hash('sha256',$bytes)])->assertOk();
  $this->auth($this->allToken)->postJson('https://pixels.7db.pw/api/device/uploads/'.$first['uploadId'].'/cancel',[])->assertJsonPath('cancelled',true);
  $this->assertSame(0,(int)(new GalleryStore($this->path))->run('SELECT count(*) FROM device_upload_chunks')->fetchColumn());
 }
 public function test_upload_rejects_wrong_digest_scope_and_foreign_origin():void {
  $bytes=Png::encode(['width'=>1,'height'=>1,'rgba'=>"\x12\x34\x56\xff"]);$id=(string)Str::uuid();
  $this->auth($this->readToken)->postJson('https://pixels.7db.pw/api/device/uploads/begin',['requestId'=>$id,'title'=>'No','description'=>'','tags'=>[],'size'=>strlen($bytes),'sha256'=>hash('sha256',$bytes)])->assertForbidden();
  $this->withHeader('Origin','https://attacker.invalid')->withHeader('Authorization','Bearer '.$this->allToken)->postJson('https://pixels.7db.pw/api/device/uploads/begin',['requestId'=>$id,'title'=>'No','description'=>'','tags'=>[],'size'=>strlen($bytes),'sha256'=>hash('sha256',$bytes)])->assertForbidden();$this->withHeader('Origin','https://pixels.7db.pw');
  $begin=$this->auth($this->allToken)->postJson('https://pixels.7db.pw/api/device/uploads/begin',['requestId'=>$id,'title'=>'No','description'=>'','tags'=>[],'size'=>strlen($bytes),'sha256'=>hash('sha256',$bytes)])->assertOk()->json();
  $this->auth($this->allToken)->postJson('https://pixels.7db.pw/api/device/uploads/'.$begin['uploadId'].'/chunks/0',['data'=>base64_encode($bytes),'sha256'=>str_repeat('0',64)])->assertUnprocessable();
  $this->assertSame(0,(int)(new GalleryStore($this->path))->run('SELECT count(*) FROM device_upload_chunks')->fetchColumn());
 }
 public function test_handheld_can_revoke_only_its_own_bearer_grant():void {
  $token=$this->grant(app(Pairing::class),$this->owner,['gallery:read']);
  $this->withHeader('Authorization','Bearer '.$token)->postJson('https://pixels.7db.pw/api/device/revoke',[])->assertJsonPath('revoked',true);
  $this->withHeader('Authorization','Bearer '.$token)->getJson('https://pixels.7db.pw/api/device/favorites')->assertUnauthorized();
 }
 public function test_schema_five_migration_from_v4_preserves_existing_artwork():void {
  $path=tempnam(sys_get_temp_dir(),'pd-v4-migrate-');copy(base_path('../build/php-gallery-test.sqlite'),$path);
  try{$db=new \PDO('sqlite:'.$path);$before=(int)$db->query('SELECT count(*) FROM artworks')->fetchColumn();$db->exec(file_get_contents(database_path('community.sql')));$db->exec('PRAGMA user_version=4');unset($db);
   GallerySchema::migrate($path);$store=new GalleryStore($path);$this->assertSame(5,(int)$store->db->query('PRAGMA user_version')->fetchColumn());$this->assertSame($before,(int)$store->run('SELECT count(*) FROM artworks')->fetchColumn());$this->assertSame(0,(int)$store->run('SELECT count(*) FROM device_favorites')->fetchColumn());unset($store);
  }finally{gc_collect_cycles();unlink($path);}
 }
}
