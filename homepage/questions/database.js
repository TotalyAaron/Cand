async function addNote(text) {
  const res = await fetch("/api/notes", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ text: text })
  });
  const data = await res.json();
  document.getElementById("output").innerText = data.message;
}
async function createacc() {
  const output = document.getElementById("output");
  const [success, msg] = await createAccount();
  output.textContent = msg;
  return;
}
async function createAccount() {
  const regex = /^\w+$/;
  const username = document.getElementById("username").value;
  const password = document.getElementById("password").value;
  username = username.trim();
  password = password.trim();
  if (!regex.test(username)) {
    const msg = "Username can only contain letters, numbers or underscores.");
    console.error(msg);
    return [false, msg];
  }
  const res = await fetch("/api/notes", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ username: username, password: password })
  });
  return [true, "Account created successfully!"];
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
