async function addNote(text) {
  const res = await fetch("/api/notes", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ text: text })
  });
  const data = await res.json();
  document.getElementById("output").innerText = data.message;
}
async function getNote(noteid) {
  //alert("Getting note: " + noteid);
  document.getElementById("inputquestion").placeholder = noteid;
  const res = await fetch(`/api/notes?id=${encodeURIComponent(noteid)}`, {
    method: "GET",
    headers: { "Content-Type": "application/json" }
  });
  const notes = await res.json();
  //alert(JSON.stringify(notes));
  //const data = "";
  notes.forEach(note => {
    document.getElementById("output").innerText += note.text + "\n";
  });
}
