#!/usr/bin/env python3

import cv2
from ultralytics import YOLO
import json

def main():
    # Load YOLOv8n model
    model = YOLO("yolov5n.pt")
    # model = YOLO("fine-tuning/fine_tuned_yolov5n/weights/best.pt")

    server_ip = "http://192.168.8.170"

    original_positions = {}  # {class_id: (center_x, center_y)}
    current_positions = {}   # {class_id: (center_x, center_y)}

    # Open video stream (webcam or ESP32)
    # cap = cv2.VideoCapture(0)  # Replace with ESP32 stream URL if needed
    cap = cv2.VideoCapture(server_ip)

    if not cap.isOpened():
        print("Could not open video stream")
        exit()

    # Optical Flow setup
    prev_gray = None
    motion_threshold = 2.0
    frame_counter = 0

    # Overlay storage
    overlay_centers = {}    # {class_id: (center_x, center_y)}
    overlay_boxes = {}      # {class_id: (x1, y1, x2, y2)}
    overlay_labels = {}     # {class_id: (label_text, confidence)}

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        motion_detected = True if prev_gray is None else False  # Always run YOLO on first frame

        if prev_gray is not None:
            # Optical Flow calculation
            flow = cv2.calcOpticalFlowFarneback(prev_gray, gray, None,
                                                pyr_scale=0.5, levels=3, winsize=15,
                                                iterations=3, poly_n=5, poly_sigma=1.2, flags=0)
            mag, ang = cv2.cartToPolar(flow[..., 0], flow[..., 1])
            mean_magnitude = mag.mean()

            motion_detected = mean_magnitude > motion_threshold

        prev_gray = gray  # Update for next iteration

        if motion_detected:
            results = model(frame)
            boxes = results[0].boxes.xyxy.cpu().numpy()
            confidences = results[0].boxes.conf.cpu().numpy()
            class_ids = results[0].boxes.cls.cpu().numpy().astype(int)
            names = results[0].names  # Class names

            # Reset overlay storage
            overlay_centers.clear()
            overlay_boxes.clear()
            overlay_labels.clear()

            for box, conf, class_id in zip(boxes, confidences, class_ids):
                x1, y1, x2, y2 = box.astype(int)
                center_x = int((x1 + x2) / 2)
                center_y = int((y1 + y2) / 2)
                label_text = names[class_id]

                # Record original positions
                if frame_counter == 0 and class_id not in original_positions:
                    original_positions[class_id] = (center_x, center_y)

                # Update current positions
                current_positions[class_id] = (center_x, center_y)

                # Store overlay data
                overlay_centers[class_id] = (center_x, center_y)
                overlay_boxes[class_id] = (x1, y1, x2, y2)
                overlay_labels[class_id] = (label_text, conf)

        # Draw annotations (from overlays)
        for class_id in overlay_boxes:
            x1, y1, x2, y2 = overlay_boxes[class_id]
            center_x, center_y = overlay_centers[class_id]
            label_text, conf = overlay_labels[class_id]

            # Draw bounding box
            cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
            # Draw center point
            cv2.circle(frame, (center_x, center_y), 5, (0, 0, 255), -1)
            # Draw label, confidence, and center coordinates
            label = f"{label_text} {conf:.2f} ({center_x},{center_y})"
            cv2.putText(frame, label, (x1, y1 - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 2)

        # Save to JSON with full detection info
        original_json = {}
        current_json = {}

        for class_id in overlay_boxes:
            x1, y1, x2, y2 = overlay_boxes[class_id]
            center_x, center_y = overlay_centers[class_id]
            label_text, conf = overlay_labels[class_id]
            width = x2 - x1
            height = y2 - y1

            entry = [center_x, center_y, width, height, class_id, float(conf)]

            # Save original if exists
            if class_id in original_positions:
                orig_cx, orig_cy = original_positions[class_id]
                original_json[str(class_id)] = [orig_cx, orig_cy, width, height, class_id, float(conf)]

            # Save current
            current_json[str(class_id)] = entry

        data = {
            "original": original_json,
            "current": current_json
        }

        with open("object_positions.json", "w") as f:
            json.dump(data, f, indent=4)

        # Show the video feed with overlays
        cv2.imshow("YOLO with Optical Flow Motion Detection", frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

        frame_counter += 1

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
