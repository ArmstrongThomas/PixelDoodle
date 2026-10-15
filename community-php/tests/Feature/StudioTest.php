<?php
namespace Tests\Feature;
use Tests\TestCase;
use App\Models\User;
use App\Services\{GallerySchema,GalleryStore,Png};
use Illuminate\Foundation\Testing\RefreshDatabase;
final class StudioTest extends TestCase {
 use RefreshDatabase;
 private string $path;
 protected function setUp():void {$_ENV['AUTH_ENABLED']='true';parent::setUp();$this->path=tempnam(sys_get_temp_dir(),'pd-studio-');GallerySchema::migrate($this->path);config(['app.url'=>'https://pixels.7db.pw','app.key'=>'base64:'.base64_encode(str_repeat('t',32)),'pixels.gallery_database'=>$this->path,'pixels.web_uploads'=>true]);$this->withHeader('Origin','https://pixels.7db.pw');}
 protected function tearDown():void {unset($_ENV['AUTH_ENABLED']);parent::tearDown();gc_collect_cycles();unlink($this->path);}
 private function url(string $suffix=''):string{return 'https://pixels.7db.pw/api/account/studio'.$suffix;}
 private function body():array{return ['title'=>'Tiny art','description'=>'A real description','tags'=>['tiny'],'requestId'=>(string)\Illuminate\Support\Str::uuid(),'png'=>base64_encode(Png::encode(['width'=>1,'height'=>1,'rgba'=>"\x12\x34\x56\x78"]))];}
 public function test_profile_upload_retry_ownership_and_publication():void {
  $this->getJson($this->url())->assertUnauthorized();$u=User::factory()->create();$this->actingAs($u);
  $this->postJson($this->url('/upload'),$this->body())->assertUnprocessable();
  $p=$this->postJson($this->url('/profile'),['name'=>'Artist','bio'=>''])->assertOk()->json('id');
  $this->postJson($this->url('/profile'),['name'=>'Updated artist','bio'=>'About me'])->assertJsonPath('id',$p);
  $v=$this->body();$id=$this->postJson($this->url('/upload'),$v)->assertOk()->assertJsonPath('status','pending')->json('id');
  $this->postJson($this->url('/upload'),$v)->assertOk()->assertJsonPath('id',$id)->assertJsonPath('duplicate',true);
  $this->getJson($this->url())->assertJsonCount(1,'items')->assertJsonPath('items.0.description','A real description');
  $this->get($this->url('/'.$id.'/image'))->assertOk();$this->get('/media/'.$id.'.png')->assertNotFound();$this->getJson('/api/artworks/'.$id)->assertNotFound();
  $this->actingAs(User::factory()->create())->get($this->url('/'.$id.'/image'))->assertNotFound();
  $s=new GalleryStore($this->path);$m=new \App\Services\Moderation($s,fn($actor)=>true,fn()=>1000);$m->decide('test',$id,'approve',0);
  $this->getJson('/api/artworks/'.$id)->assertOk()->assertJsonPath('description','A real description')->assertJsonPath('artist.name','Updated artist');
  $this->get('/media/'.$id.'.png')->assertOk();
 }
 public function test_upload_validation_disabled_account_and_origin():void {
  $u=User::factory()->create();$this->actingAs($u);$this->postJson($this->url('/profile'),['name'=>'Artist','bio'=>''])->assertOk();
  $v=$this->body();$v['png']=base64_encode('not a PNG');$this->postJson($this->url('/upload'),$v)->assertUnprocessable();
  $v=$this->body();$v['tags']=['<script>'];$this->postJson($this->url('/upload'),$v)->assertUnprocessable();
  $this->withHeader('Origin','https://elsewhere.invalid')->postJson($this->url('/upload'),$this->body())->assertForbidden();$this->withHeader('Origin','https://pixels.7db.pw');
  $u->disabled=true;$u->save();$this->getJson($this->url())->assertForbidden();
 }
 public function test_browser_upload_is_hidden_and_blocked_when_disabled():void {
  config(['pixels.web_uploads'=>false]);$u=User::factory()->create();$this->actingAs($u);
  $this->getJson($this->url())->assertJsonPath('webUploadsEnabled',false);
  $this->postJson($this->url('/upload'),$this->body())->assertNotFound();
  $this->assertSame(0,(int)(new GalleryStore($this->path))->run('SELECT COUNT(*) FROM artworks')->fetchColumn());
 }
}
