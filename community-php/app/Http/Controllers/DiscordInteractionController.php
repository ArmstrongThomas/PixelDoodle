<?php
namespace App\Http\Controllers;
use App\Models\User;
use App\Services\{DiscordReview,GalleryStore};
use Illuminate\Http\Request;
use Symfony\Component\HttpKernel\Exception\HttpException;
final class DiscordInteractionController {
 public function __invoke(Request $r){
  abort_unless(config('discord.enabled')&&function_exists('sodium_crypto_sign_verify_detached'),503);
  $body=$r->getContent();abort_if(strlen($body)>65536,413);
  $timestamp=$r->header('X-Signature-Timestamp','');$signature=$r->header('X-Signature-Ed25519','');$key=(string)config('discord.public_key');
  abort_unless(preg_match('/^\d{10}$/D',$timestamp)&&abs(time()-(int)$timestamp)<=300&&preg_match('/^[a-fA-F0-9]{128}$/D',$signature)&&preg_match('/^[a-fA-F0-9]{64}$/D',$key),401);
  abort_unless(sodium_crypto_sign_verify_detached(hex2bin($signature),$timestamp.$body,hex2bin($key)),401);
  $v=json_decode($body,true);abort_unless(is_array($v)&&($v['application_id']??'')===(string)config('discord.application_id'),401);
  if(($v['type']??null)===1)return ['type'=>1];
  $reply=fn($message)=>['type'=>4,'data'=>['content'=>$message,'flags'=>64,'allowed_mentions'=>['parse'=>[]]]];
  if(!in_array($v['type']??null,[3,5],true)||($v['guild_id']??'')!==config('discord.guild_id')||($v['channel_id']??'')!==config('discord.channel_id'))return $reply('This action is not available here.');
  $roles=$v['member']['roles']??[];$discordId=$v['member']['user']['id']??'';
  $user=is_string($discordId)&&$discordId!==''?User::where('discord_id',$discordId)->where('is_moderator',true)->where('disabled',false)->whereNotNull('email_verified_at')->first():null;
  if(!$user||!is_array($roles)||!array_intersect(config('discord.roles'),$roles))return $reply('Moderator access is required. Sign in with Discord on the website and ask the administrator to grant your account access.');
  if(!preg_match('/^pd:(approve|deny|hold):([a-f0-9-]{36}):(\d{1,10})$/D',$v['data']['custom_id']??'',$parts))return $reply('Unknown review action.');
  [$unused,$action,$id,$revision]=$parts;$service=new DiscordReview(new GalleryStore(config('pixels.gallery_database')));$row=$service->row($id);
  if(!$row||!$row['message_id']||isset($v['message'])&&($v['message']['id']??'')!==$row['message_id']||(int)$row['revision']!==(int)$revision||!in_array($row['status'],['pending','held'],true))return $reply('This review has changed. Use the updated message or the website review queue.');
  if($v['type']===3&&$action==='deny')return ['type'=>9,'data'=>['custom_id'=>$v['data']['custom_id'],'title'=>'Reject artwork','components'=>[['type'=>1,'components'=>[['type'=>4,'custom_id'=>'reason','label'=>'Reason (optional)','style'=>2,'required'=>false,'max_length'=>500]]]]]];
  $reason='';if($v['type']===5){if($action!=='deny')return $reply('Invalid review form.');foreach(($v['data']['components']??[]) as $group)foreach(($group['components']??[]) as $field)if(($field['custom_id']??'')==='reason')$reason=$field['value']??'';if(!is_string($reason)||mb_strlen($reason)>500)return $reply('Keep the reason under 500 characters.');$reason=str_replace(["\r","\n","\t"],' ',$reason);}
  try{$service->moderation()->decide((string)$user->id,$id,$action,(int)$revision,$reason);}catch(HttpException $e){return $reply($e->getStatusCode()===409?'Already handled or changed. Refresh the review queue.':'This decision could not be applied.');}
  // Updating this message is an interaction response, not an outbound network call.
  return ['type'=>7,'data'=>$service->payload($service->row($id))];
 }
}
