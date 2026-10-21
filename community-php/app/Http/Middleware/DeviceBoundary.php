<?php
namespace App\Http\Middleware;
use Closure;
use Illuminate\Http\Request;
final class DeviceBoundary {
 public function handle(Request $r,Closure $next) {
  abort_unless(config('pixels.auth_enabled')&&config('pixels.device_pairing'),503);
  $origin=rtrim(config('app.url'),'/');$host=parse_url($origin,PHP_URL_HOST);$port=parse_url($origin,PHP_URL_PORT);
  abort_unless(parse_url($origin,PHP_URL_SCHEME)==='https'&&$r->getHttpHost()===$host.($port?':'.$port:''),403);
  abort_unless(!$r->headers->has('Origin')||$r->header('Origin')===$origin,403);
  abort_if($r->getQueryString()&&$r->isMethod('POST'),400);
  abort_unless($r->isMethod('GET')||$r->isJson(),415);
  if($r->isMethod('POST')){$max=str_contains($r->path(),'/chunks/')?50000:8192;abort_if((int)$r->header('Content-Length',0)>$max||strlen($r->getContent())>$max,413);}
  return $next($r);
 }
}
