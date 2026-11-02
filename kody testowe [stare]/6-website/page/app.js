const ESP32_IP = "10.160.234.99";
const WEBSOCKET_URL = `ws://${ESP32_IP}:81`;

const connectionStatus = document.getElementById("connectionStatus");
const messageList = document.getElementById("messageList");
const messageInput = document.getElementById("messageInput");
const sendMessageButton = document.getElementById("sendMessageButton");

let socket;

function connectToESP32() {
    console.log("Łączenie z:", WEBSOCKET_URL);
    connectionStatus.textContent = "Łączenie...";
    socket = new WebSocket(WEBSOCKET_URL);

    socket.addEventListener("open", () => {
        console.log("Połączono z ESP32-CAM");
        connectionStatus.textContent =  "Połączono";
    });


    socket.addEventListener("message", (event) => {
        console.log(
            "Otrzymano:",
            event.data
        );
        addMessage(event.data);
    });

    socket.addEventListener("close", () => {
        console.log("Połączenie zamknięte");
        connectionStatus.textContent =  "Rozłączony";

        setTimeout(() => {
            connectToESP32();
        }, 2000);
    });

    socket.addEventListener("error", (error) => {
        console.error(
            "Błąd WebSocket:",
            error
        );
        connectionStatus.textContent = "Błąd połączenia";
    });
}

function sendMessage() {
    const message = messageInput.value;

    if (!message.trim()) {
        return;
    }

    if (!socket || socket.readyState !== WebSocket.OPEN) {
        console.log("Brak połączenia z ESP32-CAM");
        return;
    }

    socket.send(message);
    console.log("Wysłano do ESP32-CAM:", message);
    messageInput.value = "";
}

function addMessage(message) {
	const mess = document.getElementById("message");
	if (mess) mess.remove();

    const element = document.createElement("div");
	element.id = "message";
    element.classList.add("message");
    element.textContent = message;
    messageList.appendChild(element);
}

sendMessageButton.addEventListener("click", sendMessage);

connectToESP32();