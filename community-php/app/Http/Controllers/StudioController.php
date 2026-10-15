<?php
namespace App\Http\Controllers;
use App\Models\User;
use App\Services\GalleryStore;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\RateLimiter;
use Illuminate\Support\Str;
final class StudioController {
 private function actor(Request $r):string {abort_unless($r->user(),401);$u=User::find($r->user()->id);abort_unless($u&&!$u->disabled&&$u->email_verified_at,403);return (string)$u->id;}
 private function store():GalleryStore{return new GalleryStore(config('pixels.gallery_database'));}
 public function index(Request $r):array {
  $actor=$this->actor($r);$s=$this->store();$p=$s->run('SELECT p.* FROM artists p JOIN artist_accounts o ON o.artist=p.id WHERE o.account=?',[$actor])->fetch();
  $items=$p?$s->run("SELECT a.id,a.title,a.status,a.width,a.height,d.description,d.submitted_at,(SELECT reason FROM moderation_audit WHERE artwork=a.id AND action='deny' ORDER BY id DESC LIMIT 1) AS reason FROM artworks a LEFT JOIN artwork_details d ON d.artwork=a.id WHERE a.artist=? ORDER BY a.rowid DESC LIMIT 100",[$p['id']])->fetchAll():[];
  return ['profile'=>$p?:null,'items'=>$items,'webUploadsEnabled'=>(bool)config('pixels.web_uploads')];
 }
 public function profile(Request $r):array {
  $actor=$this->actor($r);$v=$r->validate(['name'=>'required|string|max:80','bio'=>'present|nullable|string|max:500']);abort_if(preg_match('/[\x00-\x1f\x7f]/',$v['name'])||!trim($v['name']),422);
  $s=$this->store();$id=$s->transaction(function()use($s,$actor,$v){$id=$s->run('SELECT artist FROM artist_accounts WHERE account=?',[$actor])->fetchColumn();if(!$id){$id='artist-'.Str::uuid();$s->run('INSERT INTO artists VALUES (?,?,?)',[$id,trim($v['name']),trim($v['bio']??'')]);$s->run('INSERT INTO artist_accounts VALUES (?,?)',[$actor,$id]);}else $s->run('UPDATE artists SET name=?,bio=? WHERE id=?',[trim($v['name']),trim($v['bio']??''),$id]);return $id;});return ['id'=>$id];
 }
 public function upload(Request $r):array {
  abort_unless(config('pixels.web_uploads'),404);
  $actor=$this->actor($r);$v=$r->validate(['title'=>'required|string|max:120','description'=>'present|nullable|string|max:2000','tags'=>'present|array|max:8','tags.*'=>'string|regex:/^[a-z0-9-]{1,32}$/D','png'=>'required|string|max:2796204','requestId'=>'required|uuid']);
  $s=$this->store();$artist=$s->run('SELECT artist FROM artist_accounts WHERE account=?',[$actor])->fetchColumn();abort_unless($artist,422,'Create your artist profile first.');
  // A retry of an acknowledged or uncertain browser upload cannot make another post.
  $key=$actor.':'.$v['requestId'];$existing=$s->run('SELECT artwork FROM artwork_details WHERE request_id=?',[$key])->fetchColumn();if($existing)return ['id'=>$existing,'duplicate'=>true];
  abort_if(RateLimiter::tooManyAttempts('upload:'.$actor,10),429,'Please wait before uploading again.');RateLimiter::hit('upload:'.$actor,3600);
  $bytes=base64_decode($v['png'],true);abort_if($bytes===false||strlen($bytes)>2097152,422,'Choose a PNG up to 2 MiB.');
  try{return $s->ingest($bytes,$artist,$v['title'],$v['tags'],$v['description']??'',$key);}catch(\InvalidArgumentException $e){abort(422,$e->getMessage());}
 }
 public function image(string $id,Request $r){$actor=$this->actor($r);$row=$this->store()->run('SELECT a.image FROM artworks a JOIN artist_accounts o ON o.artist=a.artist WHERE a.id=? AND o.account=?',[$id,$actor])->fetch();abort_unless($row,404);return response($row['image'],200,['Content-Type'=>'image/png','Cache-Control'=>'no-store']);}
}
