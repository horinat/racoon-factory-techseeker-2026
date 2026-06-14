const ranking = document.querySelector("#ranking");
const status = document.querySelector("#status");
const buttons = document.querySelectorAll("button[data-period]");

const dateFormatter = new Intl.DateTimeFormat("ja-JP", {
  timeZone: "Asia/Tokyo",
  month: "numeric",
  day: "numeric",
  hour: "2-digit",
  minute: "2-digit",
});

async function loadRanking(period = "all") {
  status.textContent = "ランキングを読み込み中...";
  ranking.replaceChildren();

  try {
    const response = await fetch(`/api/rankings?period=${period}`);
    if (!response.ok) throw new Error("request_failed");
    const data = await response.json();

    if (data.rankings.length === 0) {
      status.textContent = "まだスコアがありません。最初の記録を作ろう！";
      return;
    }

    status.textContent = `${data.rankings.length}件のスコア`;
    data.rankings.forEach((entry, index) => {
      const item = document.createElement("li");
      item.innerHTML = `
        <span class="rank">${index + 1}</span>
        <span class="player"></span>
        <strong>${entry.score}<small> pts</small></strong>
        <time>${dateFormatter.format(new Date(entry.received_at))}</time>
      `;
      item.querySelector(".player").textContent = entry.player_name;
      ranking.append(item);
    });
  } catch {
    status.textContent = "ランキングを読み込めませんでした。";
  }
}

buttons.forEach((button) => {
  button.addEventListener("click", () => {
    buttons.forEach((item) => item.classList.remove("active"));
    button.classList.add("active");
    loadRanking(button.dataset.period);
  });
});

loadRanking();
setInterval(() => {
  const active = document.querySelector("button.active");
  loadRanking(active?.dataset.period || "all");
}, 30_000);
