<?php
use Illuminate\Support\Facades\Route;use Illuminate\Http\Request;use App\Services\Pairing;
Route::post('/api/device/pair/{operation}',function(string $operation,Request $r,Pairing $p){
 abort_unless(config('pixels.auth_enabled')&&config('pixels.device_pairing'),503);
 $origin=rtrim(config('app.url'),'/');$authority=parse_url($origin,PHP_URL_HOST).(parse_url($origin,PHP_URL_PORT)?':'.parse_url($origin,PHP_URL_PORT):'');
 abort_unless(parse_url($origin,PHP_URL_SCHEME)==='https'&&$r->getHttpHost()===$authority&&(!$r->headers->has('Origin')||$r->header('Origin')===$origin),403);
 abort_if($r->getQueryString(),400);abort_unless($r->isJson(),415);abort_if(strlen($r->getContent())>1024,413);
 $body=json_decode($r->getContent(),true);abort_unless(is_array($body)&&!array_is_list($body),400);
 $subject=(string)$r->server('REMOTE_ADDR','unknown');
 if($operation==='begin'){abort_if(array_diff(array_keys($body),['label','requestedScopes']),400);$v=$r->validate(['label'=>'required|string|max:48','requestedScopes'=>'sometimes|array|min:1|max:3','requestedScopes.*'=>'string|in:gallery:read,favorites:write,artworks:submit']);return $p->begin($v['label'],$v['requestedScopes']??['gallery:read'],$subject);}
 abort_unless($operation==='poll',404);abort_if(array_diff(array_keys($body),['deviceCode']),400);$v=$r->validate(['deviceCode'=>'required|string|size:43']);return $p->poll($v['deviceCode'],$subject);
})->where('operation','begin|poll');
