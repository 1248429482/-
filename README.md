0x0 前言
         
           写这个东西，最开始是无聊，无聊，很无聊，后面想想在windows三环直接操控VmWare里面WIN10的内存，这不就相当于实现了一套简易版的windows内存解析，对学习和了解内存是非常方便的事情，甚至说对了解整个windows内核都是一个有益的事情，因为实现这么个框架的话，我在三环就可以随意修改内核里面的数据，去验证和学习底层知识（如内存、注册表、对象管理器等等），抱着这样的目的，差不多写了两个星期，完成了一个简易版的框架(DEMO)，里面的知识仅仅需要一点点C语言、汇编、PE、保护模式、windows内核方面的知识，就能了解原理，因为里面的知识大部分都是网上有的，我只是这些代码的搬运工然后糅合成一个工程，里面我写了大量的注释，让不会的哥们也能一目了然，不说了，先上效果图。



          具体原理就直接看代码吧，写起来太麻烦了，里面的注释写的比较完善了。



0x1 效果图(三环加载无签名驱动，目标机：windows10 1909)

![image](https://github.com/user-attachments/assets/c5c8cd6e-08d0-44d7-9126-0b89256d828d)



0x2 代码注释图

![image](https://github.com/user-attachments/assets/c2b410ac-031c-41db-8f6c-f99dc0422767)

![image](https://github.com/user-attachments/assets/3335a22a-1821-4df3-9518-f64c0a92826b)



0x3 注意事项




        因为我这段时间需要准备应付面试找工作了，这个DEMO有很多未完成的地方，有可能以后会完成，有可能过了这个兴趣时间，懒得写了，有兴趣的哥们可以自行完善，上面的内存解析，我只完成了部分功能(已经可以完成我目前的需求)，pagefile.sys/subsection/win10的压缩内存解析我暂时没写,这个解析不难，麻烦的是需要调用VmWare的磁盘API去读扇区然后解析NTFS，这个对于我来说，需要浪费很多时间，以后有空再写吧。



        所以，我走了一个捷径：关闭win10的分页文件，重启！后续完成了上面的NTFS解析后再来测试这一块。
![image](https://github.com/user-attachments/assets/9257d6b4-d2e7-4dc2-b192-3528103cafbf)
![image](https://github.com/user-attachments/assets/4dc07635-5421-42e8-b0a2-120a082f5508)







未完成的地方，我都写了断点。



还有这种应用层写内核的shellcode，编译需要非常小心，按照下面即可，我也写了注释，如果不懂，你就不用动了，看那段代码即可。
![image](https://github.com/user-attachments/assets/12a862d4-8a65-43d7-9cc6-8799b341e963)



