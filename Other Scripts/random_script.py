from ultralytics import YOLO

# Load YOLOv8n model
model = YOLO("yolov8n.pt")

print(model.names)

'''
{39: 'bottle', 65: 'remote', 76: 'scissors'}
'''