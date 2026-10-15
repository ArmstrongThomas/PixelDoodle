<?php
namespace App\Http\Controllers;

use App\Models\User;
use App\Services\GalleryStore;
use App\Services\Moderation;
use Illuminate\Http\Request;

final class ModerationController
{
    private function actor(Request $request): string
    {
        $user = $request->user();
        abort_unless($user, 401);
        // Re-read the role on every request; a session does not cache staff access.
        abort_unless(User::whereKey($user->id)->where('is_moderator', true)
            ->where('disabled', false)->whereNotNull('email_verified_at')->exists(), 403);
        return (string) $user->id;
    }

    private function store(): GalleryStore
    {
        return new GalleryStore(config('pixels.gallery_database'));
    }

    private function service(GalleryStore $store): Moderation
    {
        return new Moderation($store, fn ($id) => User::whereKey($id)
            ->where('is_moderator', true)->where('disabled', false)
            ->whereNotNull('email_verified_at')->exists(), fn () => (int) floor(microtime(true) * 1000));
    }

    public function index(Request $request): array
    {
        $actor = $this->actor($request);
        $store = $this->store();
        return ['items' => $this->service($store)->list($actor)];
    }

    public function image(string $id, Request $request)
    {
        $this->actor($request);
        $row = $this->store()->run("SELECT a.image FROM artworks a JOIN moderation_queue q ON q.artwork=a.id WHERE a.id=? AND a.status IN ('pending','held')", [$id])->fetch();
        abort_unless($row, 404);
        return response($row['image'], 200, ['Content-Type' => 'image/png', 'Cache-Control' => 'no-store']);
    }

    public function decide(string $id, Request $request): array
    {
        $actor = $this->actor($request);
        $data = $request->validate([
            'action' => 'required|in:approve,deny,hold',
            'revision' => 'required|integer|min:0',
            'reason' => 'sometimes|nullable|string|max:500',
        ]);
        abort_unless(is_int($data['revision']), 422);
        $store = $this->store();
        return $this->service($store)->decide($actor, $id, $data['action'], $data['revision'], $data['reason'] ?? '');
    }
}
