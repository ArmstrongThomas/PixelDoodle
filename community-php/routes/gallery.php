<?php
use Illuminate\Support\Facades\Route;
use Illuminate\Http\Request;
use App\Services\Gallery;
Route::get('/',fn()=>response()->file(config('pixels.frontend').'/index.html',['Content-Type'=>'text/html']));
foreach(['app.js'=>'text/javascript','account.js'=>'text/javascript','studio.js'=>'text/javascript','style.css'=>'text/css'] as $file=>$type) Route::get('/'.$file,fn()=>response()->file(config('pixels.frontend').'/'.$file,['Content-Type'=>$type]));
Route::get('/api/gallery',fn(Request $r,Gallery $g)=>$g->gallery($r->query()));
Route::get('/api/artworks/{id}',fn(string $id,Gallery $g)=>$g->artwork($id))->where('id','[a-z0-9-]+');
Route::get('/api/artists/{id}',fn(string $id,Gallery $g)=>$g->artist($id))->where('id','[a-z0-9-]+');
foreach(['media'=>false,'thumbs'=>true] as $prefix=>$thumb) Route::get('/'.$prefix.'/{id}.png',function(string $id,Request $r,Gallery $g)use($thumb){return response($g->image($id,$thumb),200,['Content-Type'=>'image/png']+($r->has('download')?['Content-Disposition'=>'attachment; filename="'.$id.'.png"']:[]));})->where('id','[a-z0-9-]+');
if(config('pixels.auth_enabled'))Route::middleware([\App\Http\Middleware\AccountBoundary::class,'web'])->group(base_path('routes/accounts.php'));
else Route::get('/api/account/status',fn()=>['enabled'=>false,'providers'=>[]]);
require __DIR__.'/devices.php';
require __DIR__.'/device-community.php';
Route::post('/api/discord/interactions', \App\Http\Controllers\DiscordInteractionController::class);
Route::any('/api/{path}',fn()=>response()->json(['error'=>'This feature is not enabled during the PHP migration.'],503))->where('path','(?:auth|account|device)/.*');
