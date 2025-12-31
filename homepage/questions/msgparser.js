const tXml = require("./tXml.js");
const lbreak = "<br></br>";
function displayParsed(display, data) {
  const [result] = tXml.parse(data);
  const username = result.attributes.userdata;
  if (username == null) {
    console.error("No username is found for this post.");
    return false;
  }
  display.innerHTML = username + lbreak;
  return true;
}
