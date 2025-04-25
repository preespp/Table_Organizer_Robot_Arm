import cv2

cap = cv2.VideoCapture("http://192.168.0.170/jpg")

while True:
    ret, frame = cap.read()
    if not ret:
        break
    cv2.imshow('ESP32 Stream', frame)
    if cv2.waitKey(1) == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()