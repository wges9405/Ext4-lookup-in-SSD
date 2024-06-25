# Ext4-lookup-in-SSD

- Draw.io
  - Flow chart of data structure, request, and function path of Ext4
    - https://drive.google.com/file/d/1PxfTq9ffQ7vQGSm32q025TxkKrQS9LRX/view?usp=sharing
  - FEMU
    - https://drive.google.com/file/d/1pcd28F-QsExan1eD5VRsTB2dmw56RBb4/view?usp=sharing
- PPT
  - HTree (Ext4)
    - https://docs.google.com/presentation/d/1EgGVdzIit5I08UXyRTfS0zKMfBM18b56tkZ7Mr3Yads/edit?usp=sharing
- HackMD:
  - Researchs about subpage operation
    - https://hackmd.io/@ArRay/HkIsgzA86
  - Data structure of Ext4
    - https://hackmd.io/@ArRay/S1qM4NL53
  - Method of Share file (For understanding real data structure of Ext4)
    - https://hackmd.io/@ArRay/ry4Ckz0Up
  - How does Linux (Ext4) prepare and send request to device
    - https://hackmd.io/@ArRay/ryjk5-3ap
  - How does FEMU receive request from host
    - https://hackmd.io/@ArRay/r1vpoUapa
  - Linux(v5.4.0) IOCTL command (including admin cmd and io cmd)
    - https://hackmd.io/@ArRay/r1G0rPzx0
  - Experiment setup (Linux v5.4.0 / FEMU)
    - https://hackmd.io/@ArRay/ByfzAKfyC


Experiment:
1. Install FEMU
  -  https://github.com/MoatLab/FEMU/tree/68032f3509d61da171b65512c6b51556d3dce6f1
2. Download nvme-search.c & hash.c, put in ./femu/hs/femu/
![image](https://github.com/wges9405/Ext4-lookup-in-SSD/assets/46646964/415ec598-3b1c-4a55-8ca5-1d1ba6d447e5)
3. Run FEMU (blackbox mod), log in FEMU
4. Download linux kernel v5.4.0 (or same as linux kernel version of FEMU)
  -  https://hackmd.io/jfO4qOG9THCYjiZO5pEnNg?view#Download-Linux-kernel-540-inside-the-FEMU
5. Modify linux kernel v5.4.0
  -  https://hackmd.io/jfO4qOG9THCYjiZO5pEnNg?view#HOST
6. Compile kernel
  -  https://hackmd.io/jfO4qOG9THCYjiZO5pEnNg?view#Compile-kernel-code
7. 
