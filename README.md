## Play Demo
- 実際のランキング画面: https://racoonfactory-techseeker-2026-wvbquo-34408a-220-158-16-196.sslip.io/
- 作品説明: [とんがりショット](https://protopedia.net/prototype/8972)

# M5Stack Score Server

M5Stackからゲーム結果を受信し、PostgreSQLへ保存してランキング表示するDokploy向けアプリです。

## 作品情報

- イベント: [テックシーカーハッカソン2026](https://techseeker.jp/)
- チーム: チームU「RacoonFactory」
- テーマ: 身体を拡張するデバイス開発

## Dokployへデプロイ

1. このフォルダをGitHubなどのGitリポジトリへ登録します。
2. Dokployで `Create Project` → `Create Service` → `Docker Compose` を選択します。
3. Gitリポジトリを接続し、Compose Pathに `./docker-compose.yml` を指定します。
4. DokployのEnvironmentへ次の2つを登録します。

```env
API_KEY=十分に長いランダムな文字列
POSTGRES_PASSWORD=API_KEYとは異なる十分に長いランダムな文字列
```

秘密値の生成例:

```bash
openssl rand -hex 32
```

5. `Deploy`を実行します。
6. ComposeサービスのDomainsで、ランキング用ドメインを`app`サービスのコンテナポート`3000`へ接続し、HTTPSを有効にします。

PostgreSQLのポートはインターネットへ公開しないでください。

## M5Stackからスコア登録

`API_KEY`はM5Stack内へ保存し、HTTPヘッダーで送ります。

```http
POST /api/scores HTTP/1.1
Host: score.example.com
Authorization: Bearer YOUR_API_KEY
Content-Type: application/json

{
  "game_id": "target-01-20260614-150001",
  "device_id": "target-01",
  "player_name": "PLAYER1",
  "score": 12,
  "duration_seconds": 30,
  "started_at": "2026-06-14T15:00:01+09:00"
}
```

同じ`game_id`と`device_id`の組み合わせは二重登録されません。

## API

| Method | Path | 説明 |
|---|---|---|
| `POST` | `/api/scores` | スコア登録。Bearer APIキー必須 |
| `GET` | `/api/rankings?period=all` | 全期間ランキング |
| `GET` | `/api/rankings?period=today` | 今日のランキング（日本時間） |
| `GET` | `/api/rankings?period=week` | 直近7日ランキング |
| `GET` | `/api/health` | ヘルスチェック |

## 動作確認

```bash
curl https://score.example.com/api/health
```

```bash
curl -X POST https://score.example.com/api/scores \
  -H 'Authorization: Bearer YOUR_API_KEY' \
  -H 'Content-Type: application/json' \
  -d '{
    "game_id":"test-001",
    "device_id":"target-01",
    "player_name":"TEST PLAYER",
    "score":10,
    "duration_seconds":30,
    "started_at":"2026-06-14T15:00:00+09:00"
  }'
```
