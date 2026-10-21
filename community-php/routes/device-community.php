<?php
use Illuminate\Support\Facades\Route;
use App\Http\Controllers\DeviceCommunityController as Device;
Route::middleware([\App\Http\Middleware\DeviceBoundary::class])->group(function () {
    Route::post('/api/device/revoke', [Device::class, 'disconnect']);
    Route::get('/api/device/favorites', [Device::class, 'favorites']);
    Route::post('/api/device/favorites/{id}', [Device::class, 'favorite'])->where('id', '[a-z0-9-]{1,64}');
    Route::get('/api/device/uploads', [Device::class, 'uploads']);
    Route::post('/api/device/uploads/begin', [Device::class, 'beginUpload']);
    Route::get('/api/device/uploads/{id}', [Device::class, 'uploadStatus'])->whereUuid('id');
    Route::post('/api/device/uploads/{id}/cancel', [Device::class, 'cancelUpload'])->whereUuid('id');
    Route::post('/api/device/uploads/{id}/chunks/{number}', [Device::class, 'putChunk'])->whereUuid('id')->whereNumber('number');
    Route::post('/api/device/uploads/{id}/finalize', [Device::class, 'finalizeUpload'])->whereUuid('id');
});
