<?php
return ['gallery_database'=>env('GALLERY_DATABASE'), 'frontend'=>base_path('../community/public'),'auth_enabled'=>env('AUTH_ENABLED',false),'email_enabled'=>env('EMAIL_ENABLED',false),'device_pairing'=>env('ENABLE_DEVICE_PAIRING',false),'web_uploads'=>env('WEB_UPLOADS_ENABLED',false)];
