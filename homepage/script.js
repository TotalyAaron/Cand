const webpath = "";
document.addEventListener('DOMContentLoaded', function() {
  webpath = window.location.href;
});
function home() {
  window.location.href = webpath;
}
function questions() {
  window.location.href = "https://cand-seven.vercel.app";
}
