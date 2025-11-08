function gotoerrlist() {
  window.location.href = "https://totalyaaron.github.io/Cand/ErrorList";
  return;
}
function gotodownload() {
  window.location.href = "https://totalyaaron.github.io/Cand/archive";
  return;
}
function gotosettings() {
  window.location.href = "https://totalyaaron.github.io/Cand/settings";
  return;
}
function refreshColor() {
  let bcolor = localStorage.getItem("bcolor");
  document.body.style.backgroundColor = bcolor;
  return;
}
document.addEventListener("DOMContentLoaded", () => {
  refreshColor();
});
