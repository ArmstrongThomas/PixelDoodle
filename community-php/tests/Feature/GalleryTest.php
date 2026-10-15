<?php
namespace Tests\Feature;
use Tests\TestCase;
final class GalleryTest extends TestCase {
 protected function setUp():void {parent::setUp();config(['pixels.gallery_database'=>base_path('../build/php-gallery-test.sqlite')]);}
 public function test_gallery_contract_and_filters():void {
  $this->getJson('/api/gallery?limit=8')->assertOk()->assertJsonPath('total',12)->assertJsonPath('nextOffset',8)->assertJsonCount(8,'items');
  $this->getJson('/api/gallery?artist=demo-studio')->assertOk()->assertJsonPath('total',6);
  $this->getJson('/api/gallery?q=nonexistent')->assertOk()->assertJsonPath('total',0);
  $this->getJson('/api/gallery?limit=100')->assertStatus(400);
  $this->getJson('/api/gallery?offset=-1')->assertStatus(400);
 }
 public function test_hidden_media_metadata_and_counts():void {
  foreach(['hidden-pending','hidden-denied','hidden-held','hidden-private','missing'] as $id)foreach(['/api/artworks/','/media/','/thumbs/'] as $prefix)$this->get($prefix.$id.($prefix==='/api/artworks/'?'':'.png'))->assertNotFound();
  $this->getJson('/api/artists/demo-studio')->assertJsonPath('count',6);
 }
 public function test_image_download_and_frontend():void {
  $r=$this->get('/media/study-2.png?download=1')->assertOk()->assertHeader('Content-Type','image/png')->assertHeader('Content-Disposition','attachment; filename="study-2.png"');
  $this->assertSame(64,unpack('N',substr($r->getContent(),16,4))[1]);$this->assertSame(96,unpack('N',substr($r->getContent(),20,4))[1]);
  $this->get('/')->assertOk();$this->get('/app.js')->assertOk();$this->get('/.env')->assertNotFound();$this->get('/composer.json')->assertNotFound();
 }
 public function test_unported_mutations_fail_closed():void {
  $this->getJson('/api/account/status')->assertExactJson(['enabled'=>false,'providers'=>[]]);
  $this->postJson('/api/device/pair/begin',['label'=>'DS'])->assertStatus(503);
  $this->postJson('/api/auth/sign-in/social',[])->assertStatus(503);
  $this->postJson('/api/gallery',[])->assertStatus(405);
 }
}
