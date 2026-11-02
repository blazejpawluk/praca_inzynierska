const ESP32_IP = "10.185.10.99";
const WEBSOCKET_URL = `ws://${ESP32_IP}:81`;

const connectionStatus = document.getElementById("connectionStatus");
const sensorsStatus = document.getElementById("sensorsStatus");

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
        handleMessage(event.data);
    });

    socket.addEventListener("close", () => {
        console.log("Połączenie zamknięte");
        connectionStatus.textContent =  "Rozłączony";
		sensorsStatus.textContent = "rozłączone";

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

function sendMessage(message) {
    if (!message.trim()) {
        return;
    }

    if (!socket || socket.readyState !== WebSocket.OPEN) {
        console.log("Brak połączenia z ESP32-CAM");
        return;
    }

    socket.send(message);
    console.log("Wysłano do ESP32-CAM:", message);
}

function handleMessage(message) {
	if (message.startsWith("sensors:")) {
		sensorsStatus.textContent = "lewy: ";
		sensorsStatus.textContent += message.split(':')[1].split(',')[0].trim();
		sensorsStatus.textContent += ", prawy: ";
		sensorsStatus.textContent += message.split(',', 2)[1].trim();
	}
}

function handleGoForward() {
	sendMessage("forward");
}
const forwardButton = document.getElementById("forwardButton");
forwardButton.addEventListener("click", handleGoForward);

function handleStop() {
	sendMessage("stop");
}
const stopButton = document.getElementById("stopButton");
stopButton.addEventListener("click", handleStop);

connectToESP32();