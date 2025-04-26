from ultralytics import YOLO

# Load pre-trained model (YOLOv5n or YOLOv8n)
model = YOLO('yolov5n.pt')

# Fine-tune
model.train(data='data.yaml',
            epochs=20,
            imgsz=640,
            batch=16,
            project='fine-tuning',
            name='fine_tuned_yolov5n',
            exist_ok=True)
