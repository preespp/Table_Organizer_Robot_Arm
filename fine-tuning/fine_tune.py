from ultralytics import YOLO

def main():
    model = YOLO('yolov5n.pt')
    model.train(
        data='data.yaml',
        epochs=100,
        imgsz=640,
        device=0,          # your GTX-1070 (PyTorch index 0)
        workers=4          # or 0 if you prefer to avoid multiprocessing
    )

if __name__ == "__main__":      # ← REQUIRED on Windows when workers>0
    main()
