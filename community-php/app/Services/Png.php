<?php
namespace App\Services;
/** Strict bounded static PNG subset; preserves all eight alpha bits. */
final class Png {
 private const SIG="\x89PNG\r\n\x1a\n";
 private static function fail():never {throw new \InvalidArgumentException('Invalid or unsupported PNG');}
 private static function n(string $s,int $o=0):int {return unpack('N',substr($s,$o,4))[1];}
 private static function word(string $s,int $o=0):int {return unpack('n',substr($s,$o,2))[1];}
 public static function chunk(string $t,string $d):string {return pack('N',strlen($d)).$t.$d.hash('crc32b',$t.$d,true);}
 public static function decode(string $b):array {
  $len=strlen($b);if($len>2097152||$len<45||substr($b,0,8)!==self::SIG)self::fail();
  $w=0;$palette=$alpha=null;$started=$dataEnded=$ended=false;$parts='';$count=0;
  for($at=8;$at<$len;){if(++$count>1024||$at+12>$len)self::fail();$n=self::n($b,$at);$t=substr($b,$at+4,4);if($n>$len-$at-12||!preg_match('/^[A-Za-z]{2}[A-Z][A-Za-z]$/D',$t))self::fail();$d=substr($b,$at+8,$n);if(hash('crc32b',$t.$d,true)!==substr($b,$at+8+$n,4))self::fail();$at+=$n+12;
   if(!$w&&$t!=='IHDR')self::fail();if($started&&$t!=='IDAT')$dataEnded=true;
   if($t==='IHDR'){if($w||$n!==13)self::fail();$w=self::n($d);$h=self::n($d,4);$type=ord($d[9]);$bpp=[0=>1,2=>3,3=>1,4=>2,6=>4][$type]??0;if($w<1||$h<1||$w>512||$h>512||ord($d[8])!==8||!$bpp||substr($d,10)!=="\0\0\0")self::fail();}
   elseif($t==='PLTE'){if($palette!==null||$started||$alpha!==null||in_array($type,[0,4])||!$n||$n%3||$n>768)self::fail();$palette=$d;}
   elseif($t==='tRNS'){if($alpha!==null||$started||!in_array($type,[0,2,3])||($type===3&&($palette===null||!$n||$n>strlen($palette)/3))||($type===0&&($n!==2||self::word($d)>255))||($type===2&&($n!==6||self::word($d)>255||self::word($d,2)>255||self::word($d,4)>255)))self::fail();$alpha=$d;}
   elseif($t==='IDAT'){if($dataEnded||($type===3&&$palette===null))self::fail();$started=true;$parts.=$d;}
   elseif($t==='IEND'){if($n||!$started||$at!==$len)self::fail();$ended=true;break;}
   elseif(in_array($t,['acTL','fcTL','fdAT','iCCP','cICP','mDCV','cLLI'])||ctype_upper($t[0]))self::fail();
  }
  if(!$ended)self::fail();$stride=$w*$bpp;$expected=($stride+1)*$h;$raw=@gzuncompress($parts,$expected);if($raw===false||strlen($raw)!==$expected)self::fail();
  $z=inflate_init(ZLIB_ENCODING_DEFLATE);$check=@inflate_add($z,$parts,ZLIB_FINISH);if($check!==$raw||inflate_get_status($z)!==ZLIB_STREAM_END||inflate_get_read_len($z)!==strlen($parts))self::fail();unset($check);
  $rows=str_repeat("\0",$stride*$h);
  for($y=0;$y<$h;$y++){$filter=ord($raw[$y*($stride+1)]);if($filter>4)self::fail();for($x=0;$x<$stride;$x++){$a=$x>=$bpp?ord($rows[$y*$stride+$x-$bpp]):0;$up=$y?ord($rows[($y-1)*$stride+$x]):0;$c=$y&&$x>=$bpp?ord($rows[($y-1)*$stride+$x-$bpp]):0;$p=$a+$up-$c;$pa=abs($p-$a);$pb=abs($p-$up);$pc=abs($p-$c);$pred=match($filter){0=>0,1=>$a,2=>$up,3=>intdiv($a+$up,2),4=>$pa<=$pb&&$pa<=$pc?$a:($pb<=$pc?$up:$c)};$rows[$y*$stride+$x]=chr((ord($raw[$y*($stride+1)+1+$x])+$pred)&255);}}
  $rgba='';for($i=0;$i<$w*$h;$i++){$s=$i*$bpp;$a=255;if($type===6){$rgba.=substr($rows,$s,4);continue;}elseif($type===2){$r=ord($rows[$s]);$g=ord($rows[$s+1]);$blue=ord($rows[$s+2]);if($alpha!==null&&$r===self::word($alpha)&&$g===self::word($alpha,2)&&$blue===self::word($alpha,4))$a=0;}elseif($type===3){$idx=ord($rows[$s]);if($idx>=strlen($palette)/3)self::fail();$r=ord($palette[$idx*3]);$g=ord($palette[$idx*3+1]);$blue=ord($palette[$idx*3+2]);$a=$alpha!==null&&$idx<strlen($alpha)?ord($alpha[$idx]):255;}else{$r=$g=$blue=ord($rows[$s]);if($type===4)$a=ord($rows[$s+1]);elseif($alpha!==null&&$r===self::word($alpha))$a=0;}$rgba.=chr($r).chr($g).chr($blue).chr($a);}
  return ['width'=>$w,'height'=>$h,'rgba'=>$rgba];
 }
 public static function encode(array $a):string {$w=$a['width'];$h=$a['height'];if(!is_int($w)||!is_int($h)||$w<1||$h<1||$w>512||$h>512||strlen($a['rgba'])!==$w*$h*4)self::fail();$raw='';for($y=0;$y<$h;$y++)$raw.="\0".substr($a['rgba'],$y*$w*4,$w*4);return self::SIG.self::chunk('IHDR',pack('NNCCCCC',$w,$h,8,6,0,0,0)).self::chunk('IDAT',gzcompress($raw)).self::chunk('IEND','');}
 public static function prepare(string $bytes):array {$a=self::decode($bytes);$scale=min(1,128/max($a['width'],$a['height']));$w=max(1,(int)round($a['width']*$scale));$h=max(1,(int)round($a['height']*$scale));$rgba='';for($y=0;$y<$h;$y++)for($x=0;$x<$w;$x++){$s=(min($a['height']-1,(int)floor(($y+.5)*$a['height']/$h))*$a['width']+min($a['width']-1,(int)floor(($x+.5)*$a['width']/$w)))*4;$rgba.=substr($a['rgba'],$s,4);}return ['width'=>$a['width'],'height'=>$a['height'],'image'=>self::encode($a),'thumbnail'=>self::encode(['width'=>$w,'height'=>$h,'rgba'=>$rgba]),'thumbnailWidth'=>$w,'thumbnailHeight'=>$h];}
}
