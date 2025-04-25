import cv2
from ultralytics import YOLO

# Load YOLOv8n model
model = YOLO("yolov8n.pt")

# Open webcam
cap = cv2.VideoCapture(0)

if not cap.isOpened():
    print("Could not open webcam")
    exit()

while True:
    ret, frame = cap.read()
    if not ret:
        break

    # Run inference
    results = model(frame)

    # Annotate the frame
    annotated_frame = results[0].plot()

    # Extract detections
    boxes = results[0].boxes.xyxy.cpu().numpy()  # [[x1, y1, x2, y2], ...]
    confidences = results[0].boxes.conf.cpu().numpy()  # [conf1, conf2, ...]
    class_ids = results[0].boxes.cls.cpu().numpy().astype(int)  # [class1, class2, ...]

    for box, conf, class_id in zip(boxes, confidences, class_ids):
        x1, y1, x2, y2 = box
        # Compute center of the box
        center_x = int((x1 + x2) / 2)
        center_y = int((y1 + y2) / 2)

        # (Optional) Correct center coordinate logic (e.g., offsets)
        corrected_center_x = center_x  # Apply your correction here
        corrected_center_y = center_y  # Apply your correction here

        # Draw the center point
        cv2.circle(annotated_frame, (corrected_center_x, corrected_center_y), 5, (0, 0, 255), -1)

        # (Optional) Display the center coordinates on screen
        label = f"({corrected_center_x}, {corrected_center_y})"
        cv2.putText(annotated_frame, label, (corrected_center_x + 10, corrected_center_y),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 2)

    # Show the annotated frame
    cv2.imshow("YOLOv8 with Centers", annotated_frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
