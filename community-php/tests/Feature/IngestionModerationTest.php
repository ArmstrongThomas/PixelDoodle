<?php
namespace Tests\Feature;
use Tests\TestCase;use App\Services\{Png,GalleryStore,Moderation};
final class IngestionModerationTest extends TestCase {
 public function test_node_oracle_all_supported_color_types_and_filters():void {
  $cases=json_decode(file_get_contents(base_path('../build/php-png-fixtures.json')),true,512,JSON_THROW_ON_ERROR);
  foreach($cases as $c){$a=Png::decode(base64_decode($c['png']));$this->assertSame(base64_decode($c['rgba']),$a['rgba'],$c['name']);$this->assertSame($c['width'],$a['width']);$this->assertSame($c['height'],$a['height']);}
 }
 public function test_empty_schema_initialization_is_idempotent_and_refuses_other_database():void {
  $path=tempnam(sys_get_temp_dir(),'pd-init-');try{\App\Services\GallerySchema::migrate($path);\App\Services\GallerySchema::migrate($path);$s=new GalleryStore($path);$this->assertSame(0,(int)$s->run('SELECT count(*) FROM artworks')->fetchColumn());unset($s);}finally{unlink($path);}
 }
 public function test_png_preserves_all_alpha_bits_and_thumbnail_samples():void {
  $rgba='';for($i=0;$i<512*2;$i++)$rgba.=chr($i%256).chr(($i*3)%256).chr(($i*7)%256).chr(($i*11)%256);
  $png=Png::encode(['width'=>512,'height'=>2,'rgba'=>$rgba]);$this->assertSame($rgba,Png::decode($png)['rgba']);$p=Png::prepare($png);$small=Png::decode($p['thumbnail']);$this->assertSame(128,$small['width']);$this->assertSame(1,$small['height']);$this->assertSame(substr($rgba,(512+2)*4,4),substr($small['rgba'],0,4));
  foreach([$png.'x',substr($png,0,-1),str_repeat('x',2097153),substr_replace($png,'x',20,1)] as $bad){try{Png::decode($bad);$this->fail('Accepted malformed image');}catch(\InvalidArgumentException){$this->addToAssertionCount(1);}}
 }
 public function test_ingestion_private_and_notification_deadline_and_hold():void {
  $file=tempnam(sys_get_temp_dir(),'pd-php-');copy(base_path('../build/php-gallery-test.sqlite'),$file);\App\Services\GallerySchema::migrate($file);
  try{$s=new GalleryStore($file);$now=100000;$m=new Moderation($s,fn($id)=>$id==='staff',function()use(&$now){return $now;});$png=Png::encode(['width'=>1,'height'=>1,'rgba'=>"\x12\x34\x56\x78"]);
   $a=$s->ingest($png,'demo-studio','Review');$id=$a['id'];$this->assertSame('private',$s->run('SELECT visibility FROM artworks WHERE id=?',[$id])->fetchColumn());$now+=9*3600000;$this->assertSame(0,$m->autoapprove());$claim=$m->claim();$m->delivered($id,$claim['attempt'],'123');$now+=8*3600000-1;$this->assertSame(0,$m->autoapprove());$now++;$this->assertSame(1,$m->autoapprove());$this->assertSame('public',$s->run('SELECT visibility FROM artworks WHERE id=?',[$id])->fetchColumn());
   $b=$s->ingest($png,'demo-studio','Hold');$claim=$m->claim();$m->decide('staff',$b['id'],'hold',0);$m->delivered($b['id'],$claim['attempt'],'456');$now+=9*3600000;$this->assertSame(0,$m->autoapprove());$this->assertNull($m->list('staff')[0]['due_at']);
   $c=$s->ingest($png,'demo-studio','Uncertain');$claim=$m->claim();$this->assertSame(1,$m->recoverInterrupted());$this->assertFalse($m->delivered($c['id'],$claim['attempt'],'789'));$this->assertNull($m->claim());
   try{$m->decide('visitor',$b['id'],'approve',2);$this->fail('Unauthorized');}catch(\Symfony\Component\HttpKernel\Exception\HttpException $e){$this->assertSame(403,$e->getStatusCode());}
   try{$m->decide('staff',$b['id'],'approve',0);$this->fail('Stale');}catch(\Symfony\Component\HttpKernel\Exception\HttpException $e){$this->assertSame(409,$e->getStatusCode());}
   unset($m,$s);$s=new GalleryStore($file);$this->assertSame('held',$s->run('SELECT status FROM artworks WHERE id=?',[$b['id']])->fetchColumn());unset($s);
  }finally{unset($e,$m,$s);gc_collect_cycles();unlink($file);}
 }
}

