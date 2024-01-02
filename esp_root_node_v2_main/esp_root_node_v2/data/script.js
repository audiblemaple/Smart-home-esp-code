
// global variables
let canvas, ctx, radius = 20, centerX, centerY;

/**
 * Draws a node (circle) with text.
 * @param {number} x - The x-coordinate of the node's center.
 * @param {number} y - The y-coordinate of the node's center.
 * @param {string} text - The text to display on the node.
 * @param {string} fillColor - The fill color for the node.
 * @param {string} strokeColor - The stroke color for the node's border.
 */
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

/**
 * Draws a line between two points.
 * @param {number} x1 - The x-coordinate of the starting point.
 * @param {number} y1 - The y-coordinate of the starting point.
 * @param {number} x2 - The x-coordinate of the ending point.
 * @param {number} y2 - The y-coordinate of the ending point.
 */
function drawLine(x1, y1, x2, y2) {
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.strokeStyle = '#000000';
    ctx.stroke();
}

/**
 * Updates the node dropdown with given sub-nodes.
 * @param {Array} subNodes - An array of sub-nodes to populate in the dropdown.
 */
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

/**
 * Submits a request to the server based on selected node and action.
 */
function submit(){
    const protocol = window.location.protocol;
    const hostname = window.location.hostname;
    const url = `${protocol}//${hostname}/`;
    fetch(`${url}dev?id=${document.getElementById('nodeDropdown').value}&act=${document.getElementById('action').value}&arg=${document.getElementById('argument').value}`)
        .then(response => console.log(response))
        .catch(error => console.error('Error sending command:', error));
}

/**
 * Updates the canvas with node data, drawing nodes and lines.
 * @param {Object} data - Data containing node information.
 */
function updateNodeData(data) {
    // Assuming 'data' contains the root node ID and connected nodes
    const rootNodeId = data.nodeId;
    const connectedNodes = data.subs;

    // Update the root node information
    document.getElementById('rootNodeList').innerHTML = `<li>${rootNodeId}</li>`;

    // Clear the canvas before drawing
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Draw the root circle
    drawNode(centerX, centerY, 'Root', 'rgba(255,0,0,0.5)', '#000000');

    if (!data.subs)
        return;

    // Update the connected nodes information
    const connectedNodeList = document.getElementById('connectedNodeList');
    connectedNodeList.innerHTML = ''; // Clear existing content
    connectedNodes.forEach(sub => {
        connectedNodeList.innerHTML += `<li>${sub.nodeId}</li>`;
    });

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

/**
 * Checks the input field and adds a class if it contains text.
 * @param {Element} input - The input element to check.
 */
function checkInput(input) {
    if (input.value) {
        input.classList.add("has-text");
    } else {
        input.classList.remove("has-text");
    }
}

/**
 * Fetches node data from the mesh root node and updates the UI.
 */
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


// WebSocket related functions
function onload(event) {
    initWebSocket();
}

/**
 * Initializes the WebSocket connection and sets up event handlers.
 */
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
}


function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

/**
 * Formats the current date and time into a specific string format.
 * @returns {string} The formatted date and time as 'dd/mm/yy hh:mm:ss'.
 */
function getTimeStamp() {
    let now = new Date();

    let seconds = String(now.getSeconds()).padStart(2, '0');
    let minutes = String(now.getMinutes()).padStart(2, '0');
    let hours = String(now.getHours()).padStart(2, '0');
    let day = String(now.getDate()).padStart(2, '0');
    let month = String(now.getMonth() + 1).padStart(2, '0');
    let year = String(now.getFullYear()).slice(-2);

    return `${day}/${month}/${year} ${hours}:${minutes}:${seconds}`;
}

/**
 * Scrolls the textarea element with id 'logArea' to its bottom.
 * This is typically used to ensure the latest logged information is visible.
 */
function scrollToBottom() {
    let textarea = document.getElementById('logArea');
    textarea.scrollTop = textarea.scrollHeight;
}

/**
 * Handles incoming WebSocket messages, appending them to the 'logArea' with a timestamp.
 * @param {Object} event - The event object containing the WebSocket message data.
 */
function onMessage(event) {
    document.getElementById('logArea').textContent += `${getTimeStamp()} -> ${event.data}\n`;
    scrollToBottom();
}


