import cv2
from ultralytics import YOLO

# Load YOLOv8n model
model = YOLO("yolov8n.pt")

# Open webcam
cap = cv2.VideoCapture(0)

if not cap.isOpened():
    print("Could not open usb cam")
    exit()

while True:
    ret, frame = cap.read()
    if not ret:
        print("Failed to grab frame")
        break

    # Run YOLOv8 inference
    results = model(frame)

    # results[0].plot() returns a numpy image with detections drawn
    annotated_frame = results[0].plot()

    cv2.imshow("YOLOv8 Live", annotated_frame)

    if cv2.waitKey(1) & 0xFF == ord('a'):
        break

cap.release()
cv2.destroyAllWindows()
