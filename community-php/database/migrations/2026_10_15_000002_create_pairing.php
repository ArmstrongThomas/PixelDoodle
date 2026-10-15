<?php
use Illuminate\Database\Migrations\Migration;use Illuminate\Database\Schema\Blueprint;use Illuminate\Support\Facades\Schema;
return new class extends Migration {
 public function up():void {
 Schema::create('pairing_requests',function(Blueprint $t){$t->uuid('id')->primary();$t->string('device_hash')->unique();$t->string('user_hash')->unique();$t->string('label',48);$t->text('scopes');$t->unsignedBigInteger('expires')->index();$t->unsignedBigInteger('next_poll');$t->integer('interval_ms');$t->string('state');$t->foreignId('account')->nullable()->constrained('users');});
 Schema::create('device_grants',function(Blueprint $t){$t->uuid('id')->primary();$t->foreignId('account')->constrained('users');$t->string('label',48);$t->string('token_hash')->unique();$t->text('scopes');$t->unsignedBigInteger('created');$t->unsignedBigInteger('expires');$t->boolean('revoked')->default(false);});
 }
 public function down():void {Schema::dropIfExists('device_grants');Schema::dropIfExists('pairing_requests');}
};
