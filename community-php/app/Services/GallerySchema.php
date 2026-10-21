<?php
namespace App\Services;
use PDO;
final class GallerySchema {
 public static function migrate(string $path):void {
  if(!is_dir(dirname($path)))throw new \RuntimeException('Create the private data directory first');
  $db=new PDO('sqlite:'.$path,null,null,[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION]);$db->exec('PRAGMA foreign_keys=ON; PRAGMA busy_timeout=5000');$v=(int)$db->query('PRAGMA user_version')->fetchColumn();
  if(!in_array($v,[0,1,2,3,4,5],true))throw new \RuntimeException('Unsupported gallery version');if($v===5)return;
  if($v===0&&$db->query("SELECT count(*) FROM sqlite_master WHERE type='table'")->fetchColumn()>0)throw new \RuntimeException('Refusing a nonempty unversioned database');
  $db->exec('BEGIN IMMEDIATE');try{
   if($v===0)$db->exec(file_get_contents(database_path('gallery.sql')));
   elseif($v===1){$db->exec('CREATE TABLE derivatives(artwork TEXT PRIMARY KEY REFERENCES artworks(id) ON DELETE CASCADE,width INTEGER NOT NULL CHECK(width BETWEEN 1 AND 128),height INTEGER NOT NULL CHECK(height BETWEEN 1 AND 128),image BLOB NOT NULL CHECK(length(image) BETWEEN 1 AND 131072))');$q=$db->prepare('INSERT INTO derivatives VALUES (?,?,?,?)');foreach($db->query('SELECT id,image FROM artworks') as $r){$a=Png::prepare($r['image']);$q->bindValue(1,$r['id']);$q->bindValue(2,$a['thumbnailWidth'],PDO::PARAM_INT);$q->bindValue(3,$a['thumbnailHeight'],PDO::PARAM_INT);$q->bindValue(4,$a['thumbnail'],PDO::PARAM_LOB);$q->execute();}}
   if($v<3)$db->exec(file_get_contents(database_path('moderation.sql')));
   if($v<4)$db->exec(file_get_contents(database_path('community.sql')));
   if($v<=4)$db->exec(file_get_contents(database_path('community-v5.sql')));
   $db->exec('PRAGMA user_version=5; COMMIT');
  }catch(\Throwable $e){$db->exec('ROLLBACK');throw $e;}
 }
}
