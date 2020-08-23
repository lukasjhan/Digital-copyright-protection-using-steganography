# Digital copyright protection using steganography (WITHUS 2016)

## ABSTRACT

Digital works are easy to reproduce. That means copyrights of Digital works are difficult to protect. However using DRM is not a solution because service users hate those inconvenience. So we must provide the universal file and also protect our copyrights

## IDEA (why steganography?)

To proctect our copyrights, we decided to pinpoint the reproducer. To do so, we need to insert the user's data into file from being noticed or found. That is why steganography is suitable for us to use.

## CORE PRINCIPAL

It is crucial that only service provider has the original file. When service users request the digital file, the service provider inserts the user key which represent exact user to identify and send it to them. 

After the reproducing and redistributing incident, we can manage to get the file and identify the reproducer by comparing it with its original file.

## DEFENSE METHOD

A reproducer can compare two other produced file to locate the position of where the key is hidden. To protect from those attacks, we locate the key in random location and use noise to guarantee a minimum length to keep the key position from being easily detected.

## License

GPL-3.0 License

## Reference

Original Source Code : https://github.com/Heisenberk/StegX