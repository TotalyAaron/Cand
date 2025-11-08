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
const stitle = document.getElementById("stitle");
const buttons = document.getElementById("buttons");
const errorlb = buttons.querySelector("#ErrorButton");
const downloadlb = buttons.querySelector("#DownloadButton");
errorlb.addEventListener("mouseenter", () => {
  showSlide(0);
  stitle.textContent = "Search for errors that you may encounter here.");
});
downloadlb.addEventListener("mouseenter", () => {
  showSlide(1);
  stitle.textContent = "Download released builds of C& interpreters or source code here.";
  let img = document.createElement("img");
  img.src = "ca.png";
  img.id = "caimgp2";
});
downloadlb.addEventListener("mouseleave", () => {
  let img = document.getElementById("caimgp2");
  if (img) img.parentNode.removeChild(img);
});
