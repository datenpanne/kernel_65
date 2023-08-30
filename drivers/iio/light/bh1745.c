 
// SPDX-License-Identifier: GPL-2.0
/*
 * bh1745.c - Support for Rohm BH1745 light sensor
 *
 * Copyright (C) 2022 Corentin Teissier <corentinteiss@yahoo.fr>
 *                    Andi Shyti <andi.shyti@studenti.polito.it>
 *
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/module.h>
#define BH1745_REG_SYSTEM_CONTROL		0x40
#define BH1745_REG_MODE_CONTROL2		0x42
#define BH1745_REG_RED_DATA_LSB			0x50
#define BH1745_REG_RED_DATA_MSB			0x51
#define BH1745_REG_GREEN_DATA_LSB		0x52
#define BH1745_REG_GREEN_DATA_MSB		0x53
#define BH1745_REG_BLUE_DATA_LSB		0x54
#define BH1745_REG_BLUE_DATA_MSB		0x55
#define BH1745_REG_CLEAR_DATA_LSB		0x56
#define BH1745_REG_CLEAR_DATA_MSB		0x57
#define BH1745_REG_MANUFACTURER_ID		0x92
#define BH1745_MASK_MODE_CONTROL2_RGBC_ENABLE	BIT(4)
#define BH1745_MASK_SYSTEM_PART_ID		GENMASK(5, 0)
#define BH1745_SYSTEM_PART_ID			0x0b
#define BH1745_MANUFACTURER_ID			0xe0
static const struct iio_chan_spec bh1745_channels[] = {
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.modified = 1,
		.channel2 = IIO_MOD_LIGHT_RED,
	},
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.modified = 1,
		.channel2 = IIO_MOD_LIGHT_GREEN,
	},
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.modified = 1,
		.channel2 = IIO_MOD_LIGHT_BLUE,
	},
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.modified = 1,
		.channel2 = IIO_MOD_LIGHT_CLEAR,
	},
};
struct bh1745_data {
	struct i2c_client *client;
};
static int bh1745_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct bh1745_data *data = iio_priv(indio_dev);
	int ret;
	u8 reg;
	if (mask != IIO_CHAN_INFO_RAW || chan->type != IIO_LIGHT)
		return -EINVAL;
	switch (chan->channel2) {
	case IIO_MOD_LIGHT_RED:
		reg = BH1745_REG_RED_DATA_LSB;
		break;
	case IIO_MOD_LIGHT_GREEN:
		reg = BH1745_REG_GREEN_DATA_LSB;
		break;
	case IIO_MOD_LIGHT_BLUE:
		reg = BH1745_REG_BLUE_DATA_LSB;
		break;
	case IIO_MOD_LIGHT_CLEAR:
		reg = BH1745_REG_CLEAR_DATA_LSB;
		break;
	default:
		return -EINVAL;
	}
	ret = i2c_smbus_read_word_data(data->client, reg);
	if (ret < 0)
		return ret;
	*val = ret;
	return IIO_VAL_INT;
}
static int bh1745_init(struct bh1745_data *data)
{
	int ret;
	u8 reg;
	/* Check the manufacturer ID */
	ret = i2c_smbus_read_byte_data(data->client,
				       BH1745_REG_MANUFACTURER_ID);
	if (ret < 0)
		return ret;
	if (ret != BH1745_MANUFACTURER_ID) {
		dev_err(&data->client->dev, "Unknown device id 0x%x\n", ret);
		return -ENODEV;
	}
	dev_dbg(&data->client->dev, "Detected BH1745 device (id 0x%x)\n", ret);
	/* Check System Part ID */
	ret = i2c_smbus_read_byte_data(data->client, BH1745_REG_SYSTEM_CONTROL);
	if (ret < 0)
		return ret;
	ret &= BH1745_MASK_SYSTEM_PART_ID;
	if (ret != BH1745_SYSTEM_PART_ID) {
		dev_err(&data->client->dev,
			"BH1745 with wrong part id 0x%x\n", ret);
		return -ENODEV;
	}
	/* Enable RGBC Measurement */
	ret = i2c_smbus_read_byte_data(data->client, BH1745_REG_MODE_CONTROL2);
	if (ret < 0)
		return ret;
	reg = ret;
	reg |= BH1745_MASK_MODE_CONTROL2_RGBC_ENABLE;
	return i2c_smbus_write_byte_data(data->client,
					 BH1745_REG_MODE_CONTROL2, reg);
};
static const struct iio_info bh1745_info = {
	.read_raw = bh1745_read_raw,
};
static int bh1745_probe(struct i2c_client *client)
{
	const struct i2c_device_id *id = i2c_client_get_device_id(client);
	struct iio_dev *indio_dev;
	struct bh1745_data *data;
	int ret;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C |
				     I2C_FUNC_SMBUS_WRITE_BYTE |
				     I2C_FUNC_SMBUS_READ_BYTE_DATA))
		return -EOPNOTSUPP;
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;
	data = iio_priv(indio_dev);
	i2c_set_clientdata(client, indio_dev);
	data->client = client;
	indio_dev->info = &bh1745_info;
	indio_dev->name = id->name;
	indio_dev->channels = bh1745_channels;
	indio_dev->num_channels = ARRAY_SIZE(bh1745_channels);
	indio_dev->modes = INDIO_DIRECT_MODE;
	ret = bh1745_init(data);
	if (ret)
		return ret;
	return iio_device_register(indio_dev);
};
static void bh1745_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	int ret;
	u8 reg;
	iio_device_unregister(indio_dev);
	/* Disable RGBC Measurement */
	ret = i2c_smbus_read_byte_data(client, BH1745_REG_MODE_CONTROL2);
	if (ret < 0)
		return;
	reg = ret & ~BH1745_MASK_MODE_CONTROL2_RGBC_ENABLE;
	i2c_smbus_write_byte_data(client, BH1745_REG_MODE_CONTROL2, reg);
};
static const struct of_device_id bh1745_of_match[] = {
	{ .compatible = "rohm,bh1745", },
	{ },
};
static const struct i2c_device_id bh1745_id[] = {
	{ "bh1745", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bh1745_id);
static struct i2c_driver bh1745_driver = {
	.driver = {
		.name = "bh1745",
		.of_match_table = of_match_ptr(bh1745_of_match),
	},
	.probe_new = bh1745_probe,
	.remove = bh1745_remove,
	.id_table = bh1745_id,
};
module_i2c_driver(bh1745_driver);
MODULE_AUTHOR("Corentin Teissier <corentinteiss@yahoo.fr>");
MODULE_AUTHOR("Andi Shyti <andi.shyti@studenti.polito.it>");
MODULE_LICENSE("GPL v2");
