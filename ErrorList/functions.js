async function checkDirectoryExists(owner, repo, directoryPath, token) {
  const url = `https://api.github.com/repos/${owner}/${repo}/contents/${directoryPath}`;
  try {
    const response = await fetch(url, {
      headers: {
        'Authorization': `token ${token}`,
        'Accept': 'application/vnd.github.v3+json'
      }
    });
    if (response.status === 200 || response.status === 403) {
      console.log("Directory exists.");
      return [true, response.status];
    } else if (response.status === 404) {
      console.log("Directory ", directoryPath, " does not exist.");
      return [false, response.status];
    } else {
      console.error("Error checking directory: ", response.status, response.statusText);
      return [false, response.status];
    }
  } catch (error) {
    console.error("Network or other error: ", error);
    return [false, response.status];
  }
}
function searchEventHandler() {
  const searchInterpreterElement = document.getElementById("sinterpretere");
  searchInterpreterElement.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      console.log("Enter key pressed:", event.target.value);
      const input = searchInterpreterElement.value;
    }
  });
}
