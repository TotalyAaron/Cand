async function checkDirectoryExists(owner, repo, directoryPath, token = null) {
  const url = `https://api.github.com/repos/${owner}/${repo}/contents/${directoryPath}`;
  const headers = {
    'Accept': 'application/vnd.github.v3+json'
  };
  if (token) {
    headers['Authorization'] = `Bearer ${token}`;
  }
  try {
    const response = await fetch(url, { headers });
    if (response.status === 200 || response.status === 403) {
      console.log("Directory exists");
      return [true, response.status];
    } else if (response.status === 404) {
      console.log("Directory", directoryPath, "does not exist");
      return [false, response.status];
    } else {
      console.error("Error checking directory:", response.status, response.statusText);
      return [false, response.status];
    }
  } catch (error) {
    console.error("Network or other error:", error);
    return [false, null];
  }
}
function searchEventHandler() {
  const searchInterpreterElement = document.getElementById("sinterpretere");
  searchInterpreterElement.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      console.log("Enter key pressed:", event.target.value);
      const input = searchInterpreterElement.value;
      const owner = "TotalyAaron";
      const repo = "Cand";
      const directoryPath = "ErrorList/" + input;
      const githubToken = null;
      checkDirectoryExists(owner, repo, directoryPath, githubToken)
        .then(([exists, errcode]) => {
          if (exists) {
            window.location.href = "https://totalyaaron.github.io/Cand/ErrorList/" + input;
          } else {
            const cplaceholder = searchInterpreterElement.placeholder;
            searchInterpreterElement.placeholder = "Cannot find error code '" + input + "'.";
            setTimeout(() => {
              searchInterpreterElement.placeholder = cplaceholder
            }, 2000);
          }
      });
    }
  });
}
