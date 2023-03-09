Usage
=====

Saving an Image to Disk
-----------------------

To save an image to disk, the ``Image`` module from the Python library ``PIL`` has to be imported first.

.. code-block:: python

    from PIL import Image

After setting up your experiment environment with a PyVCAM device initialized as shown in :ref:`artiq-setup`, 
the simplest way to capture an image is to use the :meth:`get_frame() <pyvcam_driver.PyVCAM.get_frame>` function.

The :meth:`get_frame() <pyvcam_driver.PyVCAM.get_frame>` function returns a 2D Numpy array, which can be translated to 
an image with the ``Image`` module.

.. code-block:: python
    
    frame = self.pyvcam.get_frame(exp_time=1)
    im = Image.fromarray(frame)
    im.save('/abs/path/to/folder/imagename.png')

The function :meth:`get_sequence() <pyvcam_driver.PyVCAM.get_sequence>` can be used for a rapid succession of image captures,
which returns a 3D Numpy array. The above code can be modified to save selected frames.

**NOTE:** The :meth:`start_live() <pyvcam_driver.PyVCAM.start_live>` function has a parameter that allows for direct saving
to disk, but due to the large file size generated, the function only allows saving as a ``.bin`` file, which requires additional
processing to convert into visible images.