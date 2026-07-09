const fileList = document.getElementById("file-list");

function formatSize(bytes) {
  return `${Math.floor(bytes / 1024)} Ko`;
}

function renderError(message) {
  fileList.innerHTML = "";
  const item = document.createElement("li");
  item.textContent = message;
  fileList.appendChild(item);
}

function renderFiles(files) {
  fileList.innerHTML = "";

  if (!files.length) {
    const item = document.createElement("li");
    item.innerHTML = "<i>Aucun log trouve.</i>";
    fileList.appendChild(item);
    return;
  }

  files.forEach((file) => {
    const item = document.createElement("li");
    const link = document.createElement("a");
    const size = document.createElement("span");

    link.href = `/download?file=${encodeURIComponent(file.name)}`;
    link.textContent = file.name;
    size.className = "size";
    size.textContent = formatSize(file.size);

    item.appendChild(link);
    item.appendChild(size);
    fileList.appendChild(item);
  });
}

async function loadFiles() {
  try {
    const response = await fetch("/files", { cache: "no-store" });
    if (!response.ok) {
      renderError(await response.text());
      return;
    }

    const data = await response.json();
    renderFiles(data.files || []);
  } catch (error) {
    renderError("Erreur lecture SD");
  }
}

loadFiles();
