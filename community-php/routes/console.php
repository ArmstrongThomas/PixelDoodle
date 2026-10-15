<?php
\Illuminate\Support\Facades\Artisan::command('community:moderator {email} {--revoke}', function () {
    $user = \App\Models\User::where('email', strtolower(trim($this->argument('email'))))->first();
    if (!$user || !$user->email_verified_at || $user->disabled) {
        $this->error('An existing active verified account is required.');
        return 1;
    }
    $user->is_moderator = !$this->option('revoke');
    $user->save();
    $this->info($user->is_moderator ? 'Moderator role granted.' : 'Moderator role revoked.');
    return 0;
});
\Illuminate\Support\Facades\Artisan::command('community:link-discord {email} {discordId} {--unlink}', function () {
    $user = \App\Models\User::where('email', strtolower(trim($this->argument('email'))))->first();
    $discordId = trim($this->argument('discordId'));
    if (!$user || !$user->email_verified_at || $user->disabled) {
        $this->error('An existing active verified account is required.');
        return 1;
    }
    if (!preg_match('/^\d{1,32}$/D', $discordId)) {
        $this->error('A Discord user ID is required.');
        return 1;
    }
    if (!$this->option('unlink') && \App\Models\User::where('discord_id', $discordId)->where('id', '<>', $user->id)->exists()) {
        $this->error('That Discord ID is already linked to another account.');
        return 1;
    }
    $user->discord_id = $this->option('unlink') ? null : $discordId;
    $user->save();
    $this->info($this->option('unlink') ? 'Discord account unlinked.' : 'Discord account linked.');
    return 0;
})->purpose('Administratively connect a verified website account to one Discord user ID');
\Illuminate\Support\Facades\Artisan::command('gallery:migrate',function(){
 $path=config('pixels.gallery_database');if(!$path)throw new \RuntimeException('GALLERY_DATABASE is required');
 \App\Services\GallerySchema::migrate($path);$this->info('Gallery schema ready. No demo content inserted.');
});
\Illuminate\Support\Facades\Artisan::command('gallery:ingest {file} {artist} {title} {--tag=*}',function(){
 $file=$this->argument('file');$handle=@fopen($file,'rb');if(!$handle)throw new \RuntimeException('Cannot read input');
 try{$stat=fstat($handle);if(($stat['mode']&0170000)!==0100000||$stat['size']>2097152)throw new \RuntimeException('Expected a regular PNG up to 2 MiB');$bytes=stream_get_contents($handle,2097153);}finally{fclose($handle);}
 $store=new \App\Services\GalleryStore(config('pixels.gallery_database'));$a=$store->ingest($bytes,$this->argument('artist'),$this->argument('title'),$this->option('tag'));$this->info('Imported pending/private artwork '.$a['id']);
});

use Illuminate\Foundation\Inspiring;
use Illuminate\Support\Facades\Artisan;
use Illuminate\Support\Facades\Schedule;

Artisan::command('inspire', function () {
    $this->comment(Inspiring::quote());
})->purpose('Display an inspiring quote');
\Illuminate\Support\Facades\Artisan::command('community:review-tick',function(){
 $service=new \App\Services\DiscordReview(new \App\Services\GalleryStore(config('pixels.gallery_database')));
 $this->info(json_encode($service->tick(),JSON_THROW_ON_ERROR));
})->purpose('Send queued Discord reviews, synchronize decisions and run due approvals');
Schedule::command('community:review-tick')->everyMinute()->withoutOverlapping(10);

