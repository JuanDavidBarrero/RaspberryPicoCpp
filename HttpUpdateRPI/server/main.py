from flask import Flask, Response
import os

app = Flask(__name__)
FIRMWARE_PATH = "C:/Users/JuanD/OneDrive/Documentos/PlatformIO/Projects/HttpUpdateRPI/.pio/build/rpipicow/firmware.bin"

# Asegúrate de que el archivo existe para evitar errores
if not os.path.exists(FIRMWARE_PATH):
    raise RuntimeError(f"El archivo {FIRMWARE_PATH} no se encuentra. Asegúrate de que está en la misma carpeta.")

@app.route("/firmware.bin")
def firmware():
    size = os.path.getsize(FIRMWARE_PATH)
    def generate():
        with open(FIRMWARE_PATH, "rb") as f:
            while chunk := f.read(8192):
                yield chunk
    return Response(generate(),
                    mimetype="application/octet-stream",
                    headers={"Content-Length": str(size)},
                    direct_passthrough=True)


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
