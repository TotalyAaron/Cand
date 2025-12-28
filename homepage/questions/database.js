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
  const res = await fetch("/api/notes", {
    method: "GET",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id: noteid })
  });
  const notes = await res.json();
  const data = "";
  for (const note of notes) {
    data += JSON.stringify(note, null, 2) + "\n";
  }
  document.getElementById("output").innerText = data;
}
