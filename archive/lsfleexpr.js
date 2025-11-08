const user = "TotalyAaron";
const repo = "Cand";
async function loadDir(path = "") {
  const res = await fetch(`https://api.github.com/repos/${user}/${repo}/contents/${path}`);
  if (!res.ok) {
    console.error("Failed to load directory:", res.statusText);
    return { element: null, hasContent: false };
  }
  const data = await res.json();
  // GitHub returns an error object for empty/nonexistent paths
  if (!Array.isArray(data) || data.length === 0) {
    return { element: null, hasContent: false };
  }
  const ul = document.createElement("ul");
  for (const item of data) {
    const li = document.createElement("li");
    if (item.type === "dir") {
      li.innerHTML = `<span class="folder">📁 ${item.name}</span>`;
      li.querySelector(".folder").addEventListener("click", async () => {
        if (li.querySelector("ul")) {
          li.querySelector("ul").remove();
        } else {
          const sub = await loadDir(item.path);
          if (sub.hasContent) {
            li.appendChild(sub.element);
          } else {
            const emptyMsg = document.createElement("ul");
            emptyMsg.innerHTML = "<li><em>(empty folder)</em></li>";
            li.appendChild(emptyMsg);
          }
        }
      });
    } else {
      li.innerHTML = `<button class="file-download">${item.name}</button>`;
      li.querySelector(".file-download").addEventListener("click", async () => {
      const response = await fetch(item.download_url);
      const blob = await response.blob();
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = item.name;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    });
    }
    ul.appendChild(li);
  }
  return { element: ul, hasContent: true };
}
(async () => {
  const root = await loadDir("archive/builds");
  const container = document.getElementById("repo");
  if (root.hasContent) {
    container.appendChild(root.element);
  } else {
    container.innerHTML = "<p><em>No archive of C& found.</em></p>";
  }
})();
function refreshColor() {
  let bcolor = localStorage.getItem("bcolor");
  document.body.style.backgroundColor = bcolor;
  return;
}
document.addEventListener("DOMContentLoaded", () => {
  refreshColor();
});
