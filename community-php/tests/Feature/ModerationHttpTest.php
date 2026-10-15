<?php
namespace Tests\Feature;

use App\Models\User;
use App\Services\{GalleryStore, Png};
use Illuminate\Foundation\Testing\RefreshDatabase;
use Tests\TestCase;

final class ModerationHttpTest extends TestCase
{
    use RefreshDatabase;
    private string $galleryPath;
    private string $artwork;

    protected function setUp(): void
    {
        $_ENV['AUTH_ENABLED'] = 'true';
        parent::setUp();
        $this->galleryPath = tempnam(sys_get_temp_dir(), 'pd-review-');
        copy(base_path('../build/php-gallery-test.sqlite'), $this->galleryPath);
        \App\Services\GallerySchema::migrate($this->galleryPath);
        config(['app.url' => 'https://pixels.7db.pw', 'app.key' => 'base64:'.base64_encode(str_repeat('t', 32)), 'pixels.gallery_database' => $this->galleryPath]);
        $store = new GalleryStore($this->galleryPath);
        $this->artwork = $store->ingest(Png::encode(['width'=>1,'height'=>1,'rgba'=>"\x11\x22\x33\x44"]), 'demo-studio', 'Private review')['id'];
        $this->withHeader('Origin', 'https://pixels.7db.pw');
    }

    protected function tearDown(): void
    {
        unset($_ENV['AUTH_ENABLED']);
        parent::tearDown();
        gc_collect_cycles();
        unlink($this->galleryPath);
    }

    private function url(string $suffix = ''): string
    {
        return 'https://pixels.7db.pw/api/account/moderation'.$suffix;
    }

    public function test_private_review_requires_live_staff_role(): void
    {
        $this->getJson($this->url())->assertUnauthorized();
        $user = User::factory()->create();
        $this->actingAs($user)->getJson($this->url())->assertForbidden();
        $this->get($this->url('/'.$this->artwork.'/image'))->assertForbidden();
        $this->artisan('community:moderator', ['email'=>$user->email])->assertSuccessful();
        $this->getJson($this->url())->assertOk()->assertJsonCount(1, 'items')->assertJsonMissingPath('items.0.attempt');
        $this->get($this->url('/'.$this->artwork.'/image'))->assertOk()->assertHeader('Content-Type', 'image/png');
        $this->get('/media/'.$this->artwork.'.png')->assertNotFound();
        $this->artisan('community:moderator', ['email'=>$user->email,'--revoke'=>true])->assertSuccessful();
        $this->getJson($this->url())->assertForbidden();
    }

    public function test_decisions_are_explicit_audited_and_revision_checked(): void
    {
        $user = User::factory()->create(['is_moderator'=>true]);
        $this->actingAs($user);
        $url = $this->url('/'.$this->artwork.'/decide');
        $this->get($url)->assertStatus(503);
        $this->postJson($url, ['action'=>'hold','revision'=>0,'accountId'=>'forged'])->assertJsonPath('status','held');
        $this->postJson($url, ['action'=>'approve','revision'=>0])->assertStatus(409);
        $this->postJson($url, ['action'=>'approve','revision'=>1])->assertJsonPath('status','published');
        $this->get('/media/'.$this->artwork.'.png')->assertOk();
        $store = new GalleryStore($this->galleryPath);
        $this->assertSame([(string)$user->id, (string)$user->id], $store->run('SELECT actor FROM moderation_audit ORDER BY id')->fetchAll(\PDO::FETCH_COLUMN));
    }

    public function test_administrator_can_link_discord_id_to_an_existing_verified_account(): void
    {
        $user = User::factory()->create();
        $this->artisan('community:link-discord', ['email'=>$user->email,'discordId'=>'123456789012345678'])->assertSuccessful();
        $this->assertDatabaseHas('users', ['id'=>$user->id,'discord_id'=>'123456789012345678']);
        $other = User::factory()->create();
        $this->artisan('community:link-discord', ['email'=>$other->email,'discordId'=>'123456789012345678'])->assertFailed();
        $this->artisan('community:link-discord', ['email'=>$user->email,'discordId'=>'123456789012345678','--unlink'=>true])->assertSuccessful();
        $this->assertDatabaseHas('users', ['id'=>$user->id,'discord_id'=>null]);
    }
}

