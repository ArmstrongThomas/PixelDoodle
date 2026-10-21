<?php
namespace App\Http\Controllers;

use App\Models\User;
use App\Services\Gallery;
use App\Services\GalleryStore;
use App\Services\Pairing;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\RateLimiter;
use Illuminate\Support\Str;
use PDO;

final class DeviceCommunityController
{
    private function actor(Request $r, Pairing $pairing, string $scope): array
    {
        $authorization = (string)$r->header('Authorization', '');
        abort_unless(preg_match('/^Bearer ([A-Za-z0-9_-]{43})$/D', $authorization, $m), 401);
        $actor = $pairing->authenticateGrant($m[1]);
        abort_unless($actor, 401, 'This device grant is expired or revoked. Pair again from the website.');
        abort_unless(in_array($scope, $actor['scopes'], true), 403, 'This device grant lacks the required scope. Re-pair from the website.');
        unset($actor['scopes']);
        return $actor;
    }

    private function store(): GalleryStore { return new GalleryStore(config('pixels.gallery_database')); }

    public function favorites(Request $r, Pairing $pairing, Gallery $gallery): array
    {
        $actor = $this->actor($r, $pairing, 'favorites:write');
        $limit = filter_var($r->query('limit', 12), FILTER_VALIDATE_INT);
        $offset = filter_var($r->query('offset', 0), FILTER_VALIDATE_INT);
        $compact=(string)$r->query('compact','0');abort_unless(in_array($compact,['0','1'],true),400,'Invalid compact flag.');
        abort_if($limit === false || $offset === false, 400, 'Invalid favorites page');
        $page=$gallery->favorites($actor['accountId'], $limit, $offset);
        if($compact==='1')$page['items']=array_map(static fn($a)=>['id'=>$a['id'],'title'=>$a['title'],'artist'=>['id'=>$a['artist']['id'],'name'=>$a['artist']['name']],'width'=>$a['width'],'height'=>$a['height'],'thumbnail'=>$a['thumbnail']],$page['items']);
        return $page;
    }

    public function favorite(string $id, Request $r, Pairing $pairing): array
    {
        $actor = $this->actor($r, $pairing, 'favorites:write');
        abort_unless(preg_match('/^[a-z0-9-]{1,64}$/D', $id), 404);
        abort_if(array_diff(array_keys((array)$r->json()->all()), ['favorite']), 400);
        $v = $r->validate(['favorite' => 'required|boolean']);
        $s = $this->store();
        $public = $s->run("SELECT id FROM artworks WHERE id=? AND status='published' AND visibility='public'", [$id])->fetchColumn();
        abort_unless($public, 404);
        if ($v['favorite']) {
            $s->run('INSERT OR IGNORE INTO device_favorites(account,artwork,created) VALUES (?,?,?)', [$actor['accountId'], $id, (int)floor(microtime(true)*1000)]);
        } else {
            $s->run('DELETE FROM device_favorites WHERE account=? AND artwork=?', [$actor['accountId'], $id]);
        }
        return ['id' => $id, 'favorite' => (bool)$v['favorite']];
    }

    public function disconnect(Request $r, Pairing $pairing): array
    {
        abort_if(strlen($r->getContent())>1024,413);
        $authorization=(string)$r->header('Authorization','');abort_unless(preg_match('/^Bearer ([A-Za-z0-9_-]{43})$/D',$authorization,$m),401);
        $actor=null;foreach(['gallery:read','favorites:write','artworks:submit'] as $scope){$actor=$pairing->authenticate($m[1],$scope);if($actor)break;}
        abort_unless($actor,401,'This device grant is expired or revoked.');
        return ['revoked'=>$pairing->revoke($actor['accountId'],$actor['deviceId'])];
    }

    public function uploads(Request $r, Pairing $pairing): array
    {
        $actor = $this->actor($r, $pairing, 'artworks:submit');
        $s = $this->store();
        $artist = $s->run('SELECT artist FROM artist_accounts WHERE account=?', [$actor['accountId']])->fetchColumn();
        if (!$artist) return ['profileRequired' => true, 'items' => [], 'nextOffset' => null];
        $offset = filter_var($r->query('offset', 0), FILTER_VALIDATE_INT);
        abort_if($offset === false || $offset < 0 || $offset > 10000, 400, 'Invalid uploads page');
        $total=(int)$s->run('SELECT count(*) FROM artworks WHERE artist=?',[$artist])->fetchColumn();
        $rows = $s->run("SELECT a.id,a.title,a.status,a.width,a.height,d.description,d.submitted_at,(SELECT reason FROM moderation_audit WHERE artwork=a.id AND action='deny' ORDER BY id DESC LIMIT 1) AS reason FROM artworks a LEFT JOIN artwork_details d ON d.artwork=a.id WHERE a.artist=? ORDER BY a.rowid DESC LIMIT 6 OFFSET ?", [$artist, $offset])->fetchAll();
        foreach($rows as &$row){$row['reason']=mb_strcut((string)($row['reason']??''),0,96,'UTF-8');unset($row['description'],$row['submitted_at']);}unset($row);
        return ['items' => $rows, 'total'=>$total, 'nextOffset' => $offset+count($rows)<$total ? $offset+count($rows) : null];
    }

    public function uploadStatus(string $id, Request $r, Pairing $pairing): array
    {
        $actor=$this->actor($r,$pairing,'artworks:submit');$s=$this->store();
        $row=$s->run('SELECT id,expected_size,chunk_count,expires,state,artwork FROM device_uploads WHERE id=? AND account=?',[$id,$actor['accountId']])->fetch();abort_unless($row,404);
        $received=array_map('intval',array_column($s->run('SELECT number FROM device_upload_chunks WHERE upload=? ORDER BY number',[$id])->fetchAll(),'number'));
        return ['uploadId'=>$id,'state'=>$row['state'],'receivedChunks'=>$received,'missingChunks'=>array_values(array_diff(range(0,(int)$row['chunk_count']-1),$received)),'expiresAt'=>(int)$row['expires'],'artworkId'=>$row['artwork']];
    }

    public function cancelUpload(string $id, Request $r, Pairing $pairing): array
    {
        $actor=$this->actor($r,$pairing,'artworks:submit');abort_if(strlen($r->getContent())>1024,413);
        $s=$this->store();$s->transaction(function()use($s,$id,$actor){$row=$s->run('SELECT state FROM device_uploads WHERE id=? AND account=?',[$id,$actor['accountId']])->fetch();abort_unless($row,404);abort_unless($row['state']==='receiving',409,'A finalized submission cannot be cancelled.');$s->run('DELETE FROM device_uploads WHERE id=? AND account=?',[$id,$actor['accountId']]);});
        return ['uploadId'=>$id,'cancelled'=>true];
    }

    public function beginUpload(Request $r, Pairing $pairing): array
    {
        $actor = $this->actor($r, $pairing, 'artworks:submit');
        abort_if(strlen($r->getContent()) > 8192, 413);
        abort_if(array_diff(array_keys((array)$r->json()->all()), ['requestId','title','description','tags','size','sha256']), 400);
        $v = $r->validate([
            'requestId'=>'required|uuid','title'=>'required|string|max:120','description'=>'present|nullable|string|max:2000',
            'tags'=>'present|array|max:8','tags.*'=>'string|regex:/^[a-z0-9-]{1,32}$/D',
            'size'=>'required|integer|min:1|max:2097152','sha256'=>'required|string|regex:/^[a-f0-9]{64}$/D',
        ]);
        abort_if(preg_match('/[\\x00-\\x1f\\x7f]/', $v['title']) || !trim($v['title']), 422, 'Invalid title');
        $s = $this->store(); $artist = $s->run('SELECT artist FROM artist_accounts WHERE account=?', [$actor['accountId']])->fetchColumn();
        abort_unless($artist, 422, 'Create an artist profile on the website before submitting.');
        $key = strtolower($v['requestId']); $now = (int)floor(microtime(true)*1000);
        $existing = $s->run('SELECT * FROM device_uploads WHERE account=? AND request_id=?', [$actor['accountId'], $key])->fetch();
        if ($existing) {
            abort_unless((int)$existing['expected_size'] === $v['size'] && hash_equals($existing['expected_sha256'], $v['sha256']) && $existing['title'] === trim($v['title']) && $existing['description'] === trim($v['description']??'') && $existing['tags'] === json_encode(array_values(array_unique($v['tags']))), 409, 'Request ID was already used for different artwork metadata or bytes.');
            return ['uploadId'=>$existing['id'],'chunkSize'=>32768,'chunkCount'=>(int)$existing['chunk_count'],'expiresAt'=>(int)$existing['expires'],'duplicate'=>true,'state'=>$existing['state']];
        }
        abort_if(RateLimiter::tooManyAttempts('device-upload:'.$actor['accountId'], 10), 429);
        $s->transaction(function() use ($s,$actor,$artist,$v,$key,$now) {
            $s->run("DELETE FROM device_uploads WHERE expires<=? AND state='receiving'", [$now]);
            abort_if((int)$s->run("SELECT count(*) FROM device_uploads WHERE account=? AND state='receiving'",[$actor['accountId']])->fetchColumn() >= 5, 429, 'Finish or cancel an existing upload first.');
            abort_if((int)$s->run("SELECT count(*) FROM device_uploads WHERE state='receiving'")->fetchColumn() >= 128, 503, 'The upload queue is temporarily full.');
            abort_if((int)$s->run("SELECT coalesce(sum(expected_size),0) FROM device_uploads WHERE state='receiving'")->fetchColumn() + $v['size'] > 268435456, 503, 'The temporary upload capacity is full.');
            $id=(string)Str::uuid();$count=(int)ceil($v['size']/32768);
            $s->run("INSERT INTO device_uploads(id,account,artist,request_id,title,description,tags,expected_size,expected_sha256,chunk_size,chunk_count,created,expires,state) VALUES (?,?,?,?,?,?,?,?,?,32768,?,?,?,'receiving')",[$id,$actor['accountId'],$artist,$key,trim($v['title']),trim($v['description']??''),json_encode(array_values(array_unique($v['tags']))),$v['size'],$v['sha256'],$count,$now,$now+86400000]);
        });
        RateLimiter::hit('device-upload:'.$actor['accountId'],3600);
        $row=$s->run('SELECT id,chunk_count,expires FROM device_uploads WHERE account=? AND request_id=?',[$actor['accountId'],$key])->fetch();
        return ['uploadId'=>$row['id'],'chunkSize'=>32768,'chunkCount'=>(int)$row['chunk_count'],'expiresAt'=>(int)$row['expires'],'duplicate'=>false,'state'=>'receiving'];
    }

    public function putChunk(string $id, int $number, Request $r, Pairing $pairing): array
    {
        $actor=$this->actor($r,$pairing,'artworks:submit');abort_if(strlen($r->getContent())>50000,413);
        abort_if(array_diff(array_keys((array)$r->json()->all()),['data','sha256']),400);
        $v=$r->validate(['data'=>'required|string|max:43692','sha256'=>'required|string|regex:/^[a-f0-9]{64}$/D']);
        $bytes=base64_decode($v['data'],true);abort_if($bytes===false||strlen($bytes)<1||strlen($bytes)>32768||!hash_equals($v['sha256'],hash('sha256',$bytes)),422,'Invalid chunk digest or size.');
        $s=$this->store();$row=$s->run("SELECT * FROM device_uploads WHERE id=? AND account=? AND state='receiving' AND expires>?",[$id,$actor['accountId'],(int)floor(microtime(true)*1000)])->fetch();abort_unless($row,404,'Upload session expired or unavailable.');
        abort_if($number<0||$number >= (int)$row['chunk_count'],400,'Invalid chunk number.');
        $expected=$number+1<(int)$row['chunk_count']?32768:(int)$row['expected_size']-$number*32768;abort_unless(strlen($bytes)===$expected,422,'Chunk length does not match the upload plan.');
        $s->transaction(function()use($s,$id,$number,$v,$bytes) {
            $old=$s->run('SELECT sha256 FROM device_upload_chunks WHERE upload=? AND number=?',[$id,$number])->fetchColumn();
            if($old!==false){abort_unless(hash_equals((string)$old,$v['sha256']),409,'Chunk retry differs from the saved bytes.');return;}
            $q=$s->db->prepare('INSERT INTO device_upload_chunks(upload,number,sha256,bytes) VALUES (?,?,?,?)');$q->bindValue(1,$id);$q->bindValue(2,$number,PDO::PARAM_INT);$q->bindValue(3,$v['sha256']);$q->bindValue(4,$bytes,PDO::PARAM_LOB);$q->execute();
        });
        return ['uploadId'=>$id,'number'=>$number,'received'=>true];
    }

    public function finalizeUpload(string $id, Request $r, Pairing $pairing): array
    {
        $actor=$this->actor($r,$pairing,'artworks:submit');abort_if(strlen($r->getContent())>1024,413);
        $s=$this->store();$row=$s->run('SELECT * FROM device_uploads WHERE id=? AND account=?',[$id,$actor['accountId']])->fetch();abort_unless($row,404);
        if($row['state']==='finalized')return ['id'=>$row['artwork'],'duplicate'=>true,'status'=>'pending'];
        abort_unless($row['state']==='receiving'&&(int)$row['expires']>(int)floor(microtime(true)*1000),409,'Upload session expired.');
        $chunks=$s->run('SELECT number,bytes,sha256 FROM device_upload_chunks WHERE upload=? ORDER BY number',[$id])->fetchAll();abort_unless(count($chunks)===(int)$row['chunk_count'],409,'Upload is incomplete.');
        $bytes='';foreach($chunks as $i=>$chunk){abort_unless((int)$chunk['number']===$i&&hash_equals($chunk['sha256'],hash('sha256',$chunk['bytes'])),422,'A saved chunk failed its digest check.');$bytes.=$chunk['bytes'];}
        abort_unless(strlen($bytes)===(int)$row['expected_size']&&hash_equals($row['expected_sha256'],hash('sha256',$bytes)),422,'Final artwork digest does not match.');
        $requestId='device:'.$actor['accountId'].':'.$row['request_id'];
        try{$result=$s->ingest($bytes,$row['artist'],$row['title'],json_decode($row['tags'],true,512,JSON_THROW_ON_ERROR),$row['description'],$requestId);}
        catch(\InvalidArgumentException $e){$s->run("UPDATE device_uploads SET state='failed' WHERE id=?",[$id]);abort(422,$e->getMessage());}
        $s->transaction(function()use($s,$id,$result){$s->run("UPDATE device_uploads SET state='finalized',artwork=? WHERE id=? AND state='receiving'",[$result['id'],$id]);$s->run('DELETE FROM device_upload_chunks WHERE upload=?',[$id]);});
        return ['id'=>$result['id'],'duplicate'=>false,'status'=>'pending','width'=>$result['width'],'height'=>$result['height']];
    }
}
