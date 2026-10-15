<?php
namespace App\Http\Middleware;
use Closure;use Illuminate\Http\Request;
final class AccountBoundary {
 public function handle(Request $r,Closure $next){
  if(!config('pixels.auth_enabled'))return response()->json(['error'=>'Sign-in is not configured yet.'],503);
  $origin=rtrim(config('app.url'),'/');$host=parse_url($origin,PHP_URL_HOST);$port=parse_url($origin,PHP_URL_PORT);
  abort_unless(parse_url($origin,PHP_URL_SCHEME)==='https'&&$r->getHttpHost()===$host.($port?':'.$port:''),403);
  if($r->isMethod('POST')){abort_unless($r->header('Origin')===$origin,403);abort_unless($r->isJson(),415);$max=$r->path()==='api/account/studio/upload'?2820000:8192;abort_if((int)$r->header('Content-Length',0)>$max||strlen($r->getContent())>$max,413);}
  return $next($r);
 }
}
