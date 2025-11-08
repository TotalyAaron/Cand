void refreshColor() {
  let bcolor = localStorage.getItem("bcolor");
  document.body.style.backgroundColor = bcolor;
  return;
}
document.addEventListener("DOMContentLoaded", () => {
  const select = document.getElementById("bcolorselect");
  select.addEventListener("select", function() {
    localStorage.setItem("bcolor", this.value);
    refreshColor();
  });
  refreshColor();
});
