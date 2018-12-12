#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/spi/spi.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/signal.h>
#include <linux/gpio.h>
#include <linux/mm.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>

#define config_dist_IOCTL_MAGIC_NO          0xFA
#define config_dist_INIT_GPIO		           _IO(config_dist_IOCTL_MAGIC_NO,20)
#define config_dist_RELEASE_DEVICE	        _IO(config_dist_IOCTL_MAGIC_NO,25)

#define DEVICE_NAME "configdist_dev"

static u8 config_dist_debug = 0x01;
#define CONFIG_DIST_DBG(fmt, args...) \
	do{ \
		if(config_dist_debug & 0x01) \
		printk( "[DBG][config_dist]:%5d: <%s>" fmt, __LINE__,__func__,##args ); \
	}while(0)
#define CONFIG_DIST_ERR(fmt, args...) \
	do{ \
		printk( "[DBG][config_dist]:%5d: <%s>" fmt, __LINE__,__func__,##args ); \
	}while(0)

/* Huaqin add for ZQL1820-136 by zhaojunhai1 at 2018/08/22 start */
/* Huaqin add for ZQL1820-145 by zhaojunhai1 at 2018/08/19 start */
struct configdist_data {
	struct platform_device *config_dist_dev;
	struct miscdevice *miscdev;
	u32 nfc_num;
	u32 mem_num;
	u32 mem1_num;
	u32 cam_num;
	int nfc_val;
	int mem_val;
	int mem1_val;
	int cam_val;
	struct fasync_struct *async_queue;
	struct wake_lock config_dist_lock;
	struct mutex buf_lock;
	struct pinctrl *dist_pinctrl;
	struct pinctrl_state *dist_pinctrl_mem_state;
	struct pinctrl_state *dist_pinctrl_mem1_state;
	struct pinctrl_state *dist_pinctrl_cam_state;
}*g_configdist_data;

static int config_dist_init_gpio(struct configdist_data *config_dist)
{
	int err = 0;

	config_dist->dist_pinctrl_mem_state = pinctrl_lookup_state(config_dist->dist_pinctrl, "dist_mem_active");
	if (IS_ERR(config_dist->dist_pinctrl_mem_state)){
		CONFIG_DIST_ERR("look up state err\n");
		return -1;
	}
	pinctrl_select_state(config_dist->dist_pinctrl, config_dist->dist_pinctrl_mem_state);
	if (gpio_is_valid(config_dist->mem_num)) {
		err = gpio_request(config_dist->mem_num, "config_dist-mem");
		if (err) {
			CONFIG_DIST_DBG("Could not request mem gpio.\n");
			return err;
		}
	}
	else {
		CONFIG_DIST_DBG("not valid mem gpio\n");
		return -EIO;
	}
	gpio_direction_input(config_dist->mem_num);
	config_dist->mem_val = gpio_get_value(config_dist->mem_num);

	config_dist->dist_pinctrl_mem1_state = pinctrl_lookup_state(config_dist->dist_pinctrl, "dist_mem1_active");
	if (IS_ERR(config_dist->dist_pinctrl_mem1_state)){
		CONFIG_DIST_ERR("look up state err\n");
		return -1;
	}
	pinctrl_select_state(config_dist->dist_pinctrl, config_dist->dist_pinctrl_mem1_state);
	if (gpio_is_valid(config_dist->mem1_num)) {
		err = gpio_request(config_dist->mem1_num, "config_dist-mem1");
		if (err) {
			CONFIG_DIST_DBG("Could not request mem1 gpio.\n");
			return err;
		}
	}
	else {
		CONFIG_DIST_DBG("not valid mem1 gpio\n");
		return -EIO;
	}
	gpio_direction_input(config_dist->mem1_num);
	config_dist->mem1_val = gpio_get_value(config_dist->mem1_num);

	if (gpio_is_valid(config_dist->nfc_num)) {
		err = gpio_request(config_dist->nfc_num, "config_dist-nfc");

		if (err) {
			CONFIG_DIST_DBG("Could not request nfc gpio.\n");
			return err;
		}
	}
	else {
		CONFIG_DIST_DBG("not valid nfc gpio\n");
		return -EIO;
	}
	gpio_direction_input(config_dist->nfc_num);
	config_dist->nfc_val = gpio_get_value(config_dist->nfc_num);

	config_dist->dist_pinctrl_cam_state = pinctrl_lookup_state(config_dist->dist_pinctrl, "dist_cam_active");
	if (IS_ERR(config_dist->dist_pinctrl_cam_state)){
		CONFIG_DIST_ERR("look up state err\n");
		return -1;
	}
	pinctrl_select_state(config_dist->dist_pinctrl, config_dist->dist_pinctrl_cam_state);
	if (gpio_is_valid(config_dist->cam_num)) {
		err = gpio_request(config_dist->cam_num, "config_dist-cam");

		if (err) {
			CONFIG_DIST_DBG("Could not request cam gpio.\n");
			return err;
		}
	}
	else {
		CONFIG_DIST_DBG("not valid cam gpio\n");
		return -EIO;
	}
	gpio_direction_input(config_dist->cam_num);
	config_dist->cam_val = gpio_get_value(config_dist->cam_num);
	
	CONFIG_DIST_DBG("config_dist of val mem[%d] mem1[%d] nfc[%d] cam[%d]\n",
	    config_dist->mem_val, config_dist->mem1_val, config_dist->nfc_val, config_dist->cam_val);
	CONFIG_DIST_DBG("%s(..) ok! exit.\n", __FUNCTION__);
	return 0;
}
/* Huaqin add for ZQL1820-145 by zhaojunhai1 at 2018/08/19 end */
/* Huaqin add for ZQL1820-136 by zhaojunhai1 at 2018/08/22 end */

static int config_dist_free_gpio(struct configdist_data *config_dist)
{
	int err = 0;
	CONFIG_DIST_DBG("%s(..) enter.\n", __FUNCTION__);

	if (gpio_is_valid(config_dist->mem_num)) {
		gpio_free(config_dist->mem_num);
	}

	if (gpio_is_valid(config_dist->nfc_num)) {
		gpio_free(config_dist->nfc_num);
	}

	if (gpio_is_valid(config_dist->cam_num)) {
		gpio_free(config_dist->cam_num);
	}

	CONFIG_DIST_DBG("%s(..) ok! exit.\n", __FUNCTION__);

	return err;
}

/* Huaqin add for ZQL1820-136 by zhaojunhai1 at 2018/08/21 start */
static int config_dist_parse_dts(struct device *dev,struct configdist_data *config_dist)
{

	int err = 0;

	config_dist->mem_num = of_get_named_gpio(dev->of_node, "dist,mem_gpio", 0);
	config_dist->mem1_num = of_get_named_gpio(dev->of_node, "dist,mem1_gpio", 0);
	config_dist->nfc_num = of_get_named_gpio(dev->of_node, "dist,nfc_gpio", 0);
	config_dist->cam_num = of_get_named_gpio(dev->of_node, "dist,cam_gpio", 0);

	/* Huaqin add for ZQL1820-145 by zhaojunhai1 at 2018/08/19 start */
	config_dist->dist_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(config_dist->dist_pinctrl))
	{
		CONFIG_DIST_ERR("pinctrl get failed!\n");
		err = -1;
	}
	/* Huaqin add for ZQL1820-145 by zhaojunhai1 at 2018/08/19 end */

	CONFIG_DIST_DBG("config_dist of node mem[%d] mem1[%d] nfc[%d] cam[%d]\n",
	    config_dist->mem_num, config_dist->mem1_num, config_dist->nfc_num, config_dist->cam_num);

	return err;
}
/* Huaqin add for ZQL1820-136 by zhaojunhai1 at 2018/08/21 end */

static int config_dist_open(struct inode *inode,struct file *file)
{
	file->private_data = g_configdist_data;
	return 0;
}

static int config_dist_release(struct inode *inode,struct file *file)
{
	struct configdist_data *configdist = file->private_data;
	if(NULL == configdist)
	{
		return -EIO;
	}
	file->private_data = NULL;
	return 0;
}

static long config_dist_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct configdist_data *config_dist = filp->private_data;
	mutex_lock(&config_dist->buf_lock);
	switch (cmd) {
		case config_dist_INIT_GPIO:
			err = config_dist_init_gpio(config_dist);
			break;
		case config_dist_RELEASE_DEVICE:
			config_dist_free_gpio(config_dist);
			misc_deregister(config_dist->miscdev);
			break;			
		default:
			break;
	}
	mutex_unlock(&config_dist->buf_lock);
	return err;
}

static ssize_t config_dist_read(struct file* filp, char __user *buf, size_t num, loff_t *offset)
{
	ssize_t err = 0;
	u8 val = 0x03;
	u8 ubuf = 0x0;
	struct configdist_data *config_dist = filp->private_data;
	mutex_lock(&config_dist->buf_lock);

	CONFIG_DIST_DBG(" num :%zu\n",num);
	
	switch (num) {
		case 1:
			ubuf = (config_dist->nfc_val)&0xFF;
			err = copy_to_user(buf, &ubuf, 1);
			break;
		case 2:
			/* Huaqin add for ZQL1820-136 by zhaojunhai1 at 2018/08/21 start */
			ubuf = ((config_dist->mem_val<<1)|config_dist->mem1_val)&0xFF;
			err = copy_to_user(buf+1, &ubuf, 1);
			/* Huaqin add for ZQL1820-136 by zhaojunhai1 at 2018/08/21 end */
			break;
		case 3:
			/* Huaqin add for ZQL1820-145 by zhaojunhai1 at 2018/08/19 start */
			ubuf = (config_dist->cam_val)&0xFF;
			err = copy_to_user(buf+2, &ubuf, 1);
			/* Huaqin add for ZQL1820-145 by zhaojunhai1 at 2018/08/19 end */
			break;
		case 4:
			err = copy_to_user(buf+3, &val, 1);
			break;

		default:
			break;
	}
	err = num;
	CONFIG_DIST_DBG(" err:%zu ubuf:%x\n",err, ubuf);
	//CONFIG_DIST_DBG(" buf:%x-%x-%x-%x\n",buf[0], buf[1], buf[2], buf[3]);
	mutex_unlock(&config_dist->buf_lock);
	return err;

}

static const struct file_operations config_dist_fops = {
	.owner	 = THIS_MODULE,
	.open	 = config_dist_open,
	.unlocked_ioctl = config_dist_ioctl,
	.read	 = config_dist_read,
	.release = config_dist_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = config_dist_ioctl,
#endif
};

static struct miscdevice st_config_dist_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &config_dist_fops,
};

static int config_dist_probe(struct platform_device *pdev)
{
	struct configdist_data *config_distdev= NULL;
	int status = -ENODEV;
	//int i = 0;

	CONFIG_DIST_DBG("config_dist probe ing\n");
	status = misc_register(&st_config_dist_dev);
	if (status) {
		CONFIG_DIST_DBG("config_dist misc register err%d\n",status);
		return -1;	
	}

	config_distdev = kzalloc(sizeof(struct configdist_data),GFP_KERNEL);
	config_distdev->miscdev = &st_config_dist_dev;
	config_distdev->config_dist_dev = pdev;
	mutex_init(&config_distdev->buf_lock);
	wake_lock_init(&config_distdev->config_dist_lock, WAKE_LOCK_SUSPEND, "config_dist wakelock");
	status = config_dist_parse_dts(&config_distdev->config_dist_dev->dev, config_distdev);
	if (status != 0) {
		CONFIG_DIST_DBG("config_dist parse err %d\n",status);
		goto unregister_dev;
	}

	status = config_dist_init_gpio(config_distdev);
	if (status != 0) {
		CONFIG_DIST_DBG("config_dist parse err %d\n",status);
		goto unregister_dev;
	}

		
	g_configdist_data = config_distdev;
	return 0;
unregister_dev:
	misc_deregister(&st_config_dist_dev);
	kfree(config_distdev);
	return  status;
}


static const struct of_device_id config_dist_of_match[] = {
	{ .compatible = "config,dist", },
	{},
};

static const struct platform_device_id config_dist_id[] = {
	{"config_dist", 0},
	{}
};

static struct platform_driver config_dist_driver = {
	.driver = {
		.name = "config_dist",
		.of_match_table = config_dist_of_match,
	},
	.id_table = config_dist_id,
	.probe = config_dist_probe,
};

static int __init config_dist_init(void)
{
	CONFIG_DIST_DBG("config dist init \n");
	return platform_driver_register(&config_dist_driver);
}

static void __exit config_dist_exit(void)
{
	CONFIG_DIST_DBG("config dist exit \n");
	platform_driver_unregister(&config_dist_driver);
}

module_init(config_dist_init);

module_exit(config_dist_exit);

MODULE_DESCRIPTION("config distinction Driver");
MODULE_LICENSE("GPL");

