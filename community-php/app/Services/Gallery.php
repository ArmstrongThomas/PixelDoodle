<?php
namespace App\Services;
use PDO;
final class Gallery {
 private PDO $db;
 public function __construct() {
  $path=config('pixels.gallery_database');
  if (!$path || !is_file($path)) throw new \RuntimeException('Gallery database unavailable');
  $this->db=new PDO('sqlite:'.$path,null,null,[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION,PDO::ATTR_DEFAULT_FETCH_MODE=>PDO::FETCH_ASSOC]);
  $this->db->exec('PRAGMA query_only=ON; PRAGMA busy_timeout=5000');
  if (!in_array((int)$this->db->query('PRAGMA user_version')->fetchColumn(),[2,3,4],true)) throw new \RuntimeException('Unsupported gallery database');
 }
 private function all(string $sql,array $args=[]):array {$s=$this->db->prepare($sql);$s->execute($args);return $s->fetchAll();}
 private function summary(array $a):array {
  return ['id'=>$a['id'],'title'=>$a['title'],'artist'=>['id'=>$a['artist'],'name'=>$a['name'],'bio'=>$a['bio']], 'tags'=>json_decode($a['tags'],true,512,JSON_THROW_ON_ERROR),'width'=>(int)$a['width'],'height'=>(int)$a['height'],'publishedAt'=>$a['publishedAt'],'synthetic'=>(bool)$a['synthetic'],'image'=>'/media/'.$a['id'].'.png','thumbnail'=>'/thumbs/'.$a['id'].'.png','handheld'=>'/thumbs/'.$a['id'].'.png?download=1','download'=>'/media/'.$a['id'].'.png?download=1'];
 }
 private const PUBLIC_SQL="SELECT a.id,a.artist,a.title,a.tags,a.width,a.height,a.publishedAt,a.synthetic,p.name,p.bio FROM artworks a JOIN artists p ON p.id=a.artist WHERE a.status='published' AND a.visibility='public'";
 public function gallery(array $q):array {
  $search=trim((string)($q['q']??''));$tag=(string)($q['tag']??'');$artist=(string)($q['artist']??'');$limit=(string)($q['limit']??12);$offset=(string)($q['offset']??0);
  abort_if(strlen($search)>100||strlen($tag)>32||strlen($artist)>64||!ctype_digit($limit)||!ctype_digit($offset)||(int)$limit<1||(int)$limit>24||(int)$offset>10000,400,'Invalid gallery query');
  $where=" AND (?='' OR instr(lower(a.title || ' ' || a.tags || ' ' || p.name),lower(?))>0) AND (?='' OR EXISTS(SELECT 1 FROM json_each(a.tags) WHERE value=?)) AND (?='' OR a.artist=?)";
  $args=[$search,$search,$tag,$tag,$artist,$artist];
  $count=$this->all('SELECT count(*) AS total FROM ('.self::PUBLIC_SQL.$where.')',$args)[0]['total'];
  $rows=$this->all(self::PUBLIC_SQL.$where.' ORDER BY a.publishedAt DESC,a.id LIMIT '.(int)$limit.' OFFSET '.(int)$offset,$args);
  $tags=$this->all("SELECT DISTINCT j.value FROM artworks a,json_each(a.tags) j WHERE a.status='published' AND a.visibility='public' ORDER BY j.value");
  return ['version'=>1,'mode'=>'community','items'=>array_map($this->summary(...),$rows),'total'=>(int)$count,'nextOffset'=>(int)$offset+(int)$limit<$count?(int)$offset+(int)$limit:null,'tags'=>array_column($tags,'value')];
 }
 public function artwork(string $id):array {$rows=$this->all(self::PUBLIC_SQL.' AND a.id=?',[$id]);abort_unless($rows,404);return $this->summary($rows[0])+['description'=>(int)$this->db->query('PRAGMA user_version')->fetchColumn()>=4?($this->all('SELECT description FROM artwork_details WHERE artwork=?',[$id])[0]['description']??''):($rows[0]['synthetic']?'A synthetic pixel study for the Pixel Doodle community prototype.':'')];}
 public function artist(string $id):array {$rows=$this->all("SELECT p.id,p.name,p.bio,count(*) AS count, min(a.synthetic) AS synthetic FROM artists p JOIN artworks a ON a.artist=p.id WHERE p.id=? AND a.status='published' AND a.visibility='public' GROUP BY p.id",[$id]);abort_unless($rows,404);return $rows[0];}
 public function image(string $id,bool $thumb):string {$rows=$this->all($thumb?"SELECT d.image FROM derivatives d JOIN artworks a ON a.id=d.artwork WHERE a.id=? AND a.status='published' AND a.visibility='public'":"SELECT image FROM artworks WHERE id=? AND status='published' AND visibility='public'",[$id]);abort_unless($rows,404);return $rows[0]['image'];}
}
