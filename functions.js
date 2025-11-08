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
// background color
function refreshColor() {
  let bcolor = localStorage.getItem("bcolor");
  document.body.style.backgroundColor = bcolor;
  return;
}
document.addEventListener("DOMContentLoaded", () => {
  refreshColor();
});
// slide logic
const slides = document.querySelectorAll(".slide");
// show a specific image by index
function showSlide(index) {
  slides.forEach((slide, i) => {
    slide.classList.toggle("active", i === index);
  });
  return;
}
// listeners
const buttons = document.getElementById("buttons");
const errorlb = buttons.querySelector("#ErrorButton");
const downloadlb = buttons.querySelector("#DownloadButton");
errorlb.addEventListener("mouseenter", () => {
  showSlide(0);
});
downloadlb.addEventListener("mouseenter", () => {
  showSlide(1);
});
