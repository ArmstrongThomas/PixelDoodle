<?php
use Illuminate\Support\Facades\Route;use App\Http\Controllers\AccountController;
Route::get('/api/account/status',[AccountController::class,'status']);
Route::post('/api/auth/sign-in/social',[AccountController::class,'discord']);
Route::get('/api/auth/callback/discord',[AccountController::class,'callback']);
Route::get('/api/auth/email-otp/challenge',[AccountController::class,'emailCodeChallenge']);
Route::post('/api/auth/email-otp/send-verification-otp',[AccountController::class,'sendCode']);
Route::post('/api/auth/sign-in/email-otp',[AccountController::class,'verifyCode']);
Route::post('/api/auth/sign-out',[AccountController::class,'logout']);
Route::get('/api/account/moderation', [\App\Http\Controllers\ModerationController::class, 'index']);
Route::get('/api/account/moderation/{id}/image', [\App\Http\Controllers\ModerationController::class, 'image'])->whereUuid('id');
Route::post('/api/account/moderation/{id}/decide', [\App\Http\Controllers\ModerationController::class, 'decide'])->whereUuid('id');
Route::get('/api/account/devices',function(\Illuminate\Http\Request $r,\App\Services\Pairing $p){abort_unless($r->user(),401);return ['devices'=>$p->devices($r->user()->id)];});
Route::post('/api/account/pair/inspect',function(\Illuminate\Http\Request $r,\App\Services\Pairing $p){abort_unless($r->user(),401);$v=$r->validate(['code'=>'required|string|max:12']);return $p->inspect($r->user()->id,$v['code']);});
Route::post('/api/account/pair/decide',function(\Illuminate\Http\Request $r,\App\Services\Pairing $p){abort_unless($r->user(),401);$v=$r->validate(['code'=>'required|string|max:12','approve'=>'required|boolean']);abort_unless(is_bool($v['approve']),422);return $p->decide($r->user()->id,$v['code'],$v['approve']);});
Route::post('/api/account/devices/revoke',function(\Illuminate\Http\Request $r,\App\Services\Pairing $p){abort_unless($r->user(),401);$v=$r->validate(['id'=>'required|uuid']);return ['revoked'=>$p->revoke($r->user()->id,$v['id'])];});
Route::get('/api/account/studio', [\App\Http\Controllers\StudioController::class, 'index']);
Route::post('/api/account/studio/profile', [\App\Http\Controllers\StudioController::class, 'profile']);
// Retained for the future opt-in browser workflow; disabled by default in pixels.web_uploads.
Route::post('/api/account/studio/upload', [\App\Http\Controllers\StudioController::class, 'upload']);
Route::get('/api/account/studio/{id}/image', [\App\Http\Controllers\StudioController::class, 'image'])->whereUuid('id');
