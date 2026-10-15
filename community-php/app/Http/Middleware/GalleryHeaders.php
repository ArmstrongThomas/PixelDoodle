<?php
namespace App\Http\Middleware;
use Closure;use Illuminate\Http\Request;
final class GalleryHeaders {
 public function handle(Request $request,Closure $next){
  $response=$next($request);
  foreach(['Cache-Control'=>'no-store','X-Content-Type-Options'=>'nosniff','Referrer-Policy'=>'no-referrer','Content-Security-Policy'=>"default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self'; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'"] as $key=>$value)$response->headers->set($key,$value);
  return $response;
 }
}
