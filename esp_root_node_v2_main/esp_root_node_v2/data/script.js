let canvas, ctx, radius = 15, centerX, centerY;
function drawNode(x, y, text, fillColor, strokeColor) {
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, 2 * Math.PI, false);
    ctx.fillStyle = fillColor;
    ctx.fill();
    ctx.lineWidth = 2;
    ctx.strokeStyle = strokeColor;
    ctx.stroke();

    // Add node ID text (might change it to node name)
    ctx.font = "14px Arial";
    ctx.fillStyle = 'black';
    ctx.textAlign = 'center';
    ctx.fillText(text, x, y + radius + 20);
}

// Draw lines between two nodes
function drawLine(x1, y1, x2, y2) {
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.strokeStyle = '#000000';
    ctx.stroke();
}

// Get the log from the root node
// function fetchLog() {
//     let xhr = new XMLHttpRequest();
//     xhr.onreadystatechange = function() {
//         if (this.readyState === 4 && this.status === 200)
//             document.getElementById('logArea').textContent = this.responseText;
//     };
//     xhr.open('GET', '/getLog', true);
//     xhr.send();
// }
// setInterval(fetchLog, 1000);

function updateNodeDropdown(subNodes) {
    const dropdown = document.getElementById('nodeDropdown');
    dropdown.innerHTML = ''; // Clear existing options

    subNodes.forEach(subNode => {
        const option = document.createElement('option');
        option.value = subNode.nodeId;
        option.textContent = subNode.nodeId; // Assuming 'nodeId' is the property
        dropdown.appendChild(option);
    });
}

function submit(){
    const protocol = window.location.protocol;
    const hostname = window.location.hostname;
    const url = `${protocol}//${hostname}/`;
    fetch(`${url}dev?id=${document.getElementById('nodeDropdown').value}&act=${document.getElementById('action').value}&arg=${document.getElementById('argument').value}`)
        .then(response => console.log(response))
        .catch(error => console.error('Error sending command:', error));
}

function updateNodeData(data) {
    // Assuming 'data' contains the root node ID and connected nodes
    const rootNodeId = data.nodeId;
    const connectedNodes = data.subs;

    // Update the root node information
    document.getElementById('rootNodeList').innerHTML = `<li>${rootNodeId}</li>`;

    if (!data.subs)
        return;
    // Update the connected nodes information
    const connectedNodeList = document.getElementById('connectedNodeList');
    connectedNodeList.innerHTML = ''; // Clear existing content
    connectedNodes.forEach(sub => {
        connectedNodeList.innerHTML += `<li>${sub.nodeId}</li>`;
    });

    // Clear the canvas before drawing
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Draw the root circle
    drawNode(centerX, centerY, 'Root', 'rgba(255,0,0,0.5)', '#000000');

    // Calculate positions and draw connected nodes
    const totalNodes = data.subs.length;
    const angleStep = 2 * Math.PI / totalNodes;

    data.subs.forEach((sub, index) => {
        // Calculate position
        let angle = angleStep * index;
        let childX = centerX + 100 * Math.cos(angle); // 100 is the radius from the center
        let childY = centerY + 100 * Math.sin(angle);

        // Draw the child node
        drawNode(childX, childY, sub.nodeId, 'rgba(0,0,255,0.5)', '#000000');

        // Draw a line from root to child node
        drawLine(centerX, centerY, childX, childY);
    });
}

// Ensure the DOM is fully loaded before accessing elements
document.addEventListener('DOMContentLoaded', function() {
    canvas = document.getElementById('mesh_canvas');
    ctx = canvas.getContext('2d');

    centerX = canvas.width / 2;
    centerY = canvas.height / 2;

    canvas.width = window.innerWidth - 25;
    canvas.height = 400;

    fetchNodeData();
});


function checkInput(input) {
    if (input.value) {
        input.classList.add("has-text");
    } else {
        input.classList.remove("has-text");
    }
}
function fetchNodeData() {
    const protocol = window.location.protocol;
    const hostname = window.location.hostname;
    const url = `${protocol}//${hostname}/getNodes`;
    fetch(url)
        .then(response => response.json())
        .then(data => {
            if (!data){
                console.error("Error getting nodes");
                return;
            }

            updateNodeData(data);
            if (data.subs)
                updateNodeDropdown(data.subs);
        })
        .catch(error => console.error('Error fetching nodes:', error));
}

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

function getLog(){
    websocket.send("getLog");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

// When websocket is established, call the getLog() function
function onOpen(event) {
    console.log('Connection opened');
    getLog();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function formatDateTime() {
    let now = new Date();

    let seconds = String(now.getSeconds()).padStart(2, '0');
    let minutes = String(now.getMinutes()).padStart(2, '0');
    let hours = String(now.getHours()).padStart(2, '0');
    let day = String(now.getDate()).padStart(2, '0');
    let month = String(now.getMonth() + 1).padStart(2, '0');
    let year = String(now.getFullYear()).slice(-2);

    return `${day}/${month}/${year} ${hours}:${minutes}:${seconds}`;
}

// Function that receives the message from the ESP32 with the readings
function onMessage(event) {
    document.getElementById('logArea').textContent += `${formatDateTime()} -> ${event.data}\n`;
}


