<?php
return [
 'enabled'=>env('DISCORD_BOT_ENABLED',false),
 'token'=>env('DISCORD_BOT_TOKEN'),
 'public_key'=>env('DISCORD_PUBLIC_KEY'),
 'application_id'=>env('DISCORD_CLIENT_ID'),
 'guild_id'=>env('DISCORD_GUILD_ID'),
 'channel_id'=>env('DISCORD_REVIEW_CHANNEL_ID'),
 'roles'=>array_values(array_filter(array_map('trim',explode(',',env('DISCORD_MODERATOR_ROLE_IDS',''))))),
];
