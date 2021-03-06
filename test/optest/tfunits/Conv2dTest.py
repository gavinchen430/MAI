import tensorflow as tf

sess = tf.InteractiveSession()

print("-------------------With1Channels VALID------------")

#NHWC 2,2,4,1
inputData=tf.constant([[[[1.],[2],[3],[4]],[[5],[6],[7],[8]]],[[[9],[10],[11],[12]],[[13],[14],[15],[16]]]])

#HWIO 2,2,1,3
#filter=tf.constant([[[[1.],[2]],[[3],[-4]]],[[[-1],[1]],[[-1],[1]]],[[[-1],[-1]],[[1],[1]]]])
filter=tf.constant([[[[1., -1, -1]],[[2,1,-1]]],[[[3,-1,1]],[[-4,1,1]]]])
#transpose to HWIO 2,2,1,3
#filter=tf.transpose(filter, [1,2,3,0])
#bias=tf.constant([1.,2,3])

#target 2,1,3,3
target=tf.nn.conv2d(inputData, filter, [1,1,1,1], 'VALID').eval()

print(target)

print("-------------------With1Channels SAME------------")
target=tf.nn.conv2d(inputData, filter, [1,1,1,1], 'SAME').eval()
print(target)


print("-------------------WithMultiChannels VALID------------")
#NHWC 1,2,2,2
inputData=tf.constant([[[[1.,2],[3,4]],[[5,6],[7,8]]]])
#HWIO 1,1,2,2
filter=tf.constant([[[[1.,2],[3,4]]]])
#target 1,2,2,2
target=tf.nn.conv2d(inputData, filter, [1,1,1,1], 'VALID').eval()
print(target)

print("-------------------WithMultiChannels SAME------------")
#target 1,2,2,2
target=tf.nn.conv2d(inputData, filter, [1,1,1,1], 'SAME').eval()
print(target)

print("-------------------WithMultiChannels SAME BIAS------------")
#target 1,2,2,2
target=tf.nn.conv2d(inputData, filter, [1,1,1,1], 'SAME').eval()
bias=tf.constant([1.,2])
target=tf.nn.bias_add(target, bias).eval()

print(target)
